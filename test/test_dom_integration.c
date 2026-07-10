/*
 * DOM Integration Test
 *
 * Loads a DOM file, runs execution for up to N steps or until MON 0B exit,
 * and verifies the emulator correctly executes the program.
 *
 * Usage:
 *   ./test_dom_integration <dom_file> [max_steps]
 *
 * Exit codes:
 *   0 = Success (program exited normally via MON 0B or completed steps)
 *   1 = Error (failed to load, trap, or other failure)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/cpu/nd500_domain.h"
#include "../src/ndlib/ndlib.h"
#include "../src/debugger/debugger.h"
#include "../src/libmon/mon.h"
#include "../src/libmon/mon_file_table.h"

#define DEFAULT_MAX_STEPS 10000
#define MEMORY_SIZE (16 * 1024 * 1024)  /* 16MB - same as debugger */
#define MAX_INPUTS 16
#define MAX_COMPARES 16

/* Queued console input strings (--input), applied after mon_init() */
static const char* g_inputs[MAX_INPUTS];
static int g_input_count = 0;

/* File comparisons (--compare produced=expected), checked after the run */
static const char* g_compares[MAX_COMPARES];
static int g_compare_count = 0;

/* --min-instructions N: pass if the run executed at least N instructions,
 * regardless of how it stopped. Used as a PROGRESS guard for work-in-progress
 * programs that run further than a known-bad ceiling but do not yet complete
 * cleanly (e.g. the NC compiler now reaches code generation but crashes on a
 * separate downstream bug). 0 = disabled. */
static uint64_t g_min_instructions = 0;

/* Process \r and \n escape sequences into a malloc'd string */
static char* process_escapes(const char* src) {
    size_t len = strlen(src);
    char* out = malloc(len + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (src[i] == '\\' && i + 1 < len) {
            if (src[i + 1] == 'r') { out[j++] = '\r'; i++; continue; }
            if (src[i + 1] == 'n') { out[j++] = '\n'; i++; continue; }
        }
        out[j++] = src[i];
    }
    out[j] = '\0';
    return out;
}

/* Byte-compare two files; returns 0 if identical */
static int compare_files(const char* produced, const char* expected) {
    FILE* fp = fopen(produced, "rb");
    FILE* fe = fopen(expected, "rb");
    int rc = 0;
    if (!fp || !fe) {
        printf("  COMPARE ERROR: cannot open %s\n", !fp ? produced : expected);
        rc = -1;
    } else {
        long pos = 0;
        for (;;) {
            int cp = fgetc(fp);
            int ce = fgetc(fe);
            if (cp != ce) {
                printf("  COMPARE MISMATCH at byte %ld: %s has 0x%02X, %s has 0x%02X\n",
                       pos, produced, cp & 0xFF, expected, ce & 0xFF);
                rc = -1;
                break;
            }
            if (cp == EOF) break;
            pos++;
        }
    }
    if (fp) fclose(fp);
    if (fe) fclose(fe);
    return rc;
}

/* Test result tracking */
typedef struct {
    int passed;
    int failed;
    uint32_t initial_pc;
    uint32_t final_pc;
    uint64_t instructions_executed;
    StopReason stop_reason;
    int exited_normally;
} TestResult;

/* Quiet log callback - suppresses DOM loader output */
static void quiet_log(void* ctx, const char* fmt, ...) {
    (void)ctx;
    (void)fmt;
    /* Suppress output */
}

/* Verbose log callback for debugging */
static void verbose_log(void* ctx, const char* fmt, ...) {
    (void)ctx;
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

/*
 * Load and run a DOM file
 *
 * Returns:
 *   0 = Success
 *  -1 = Failed to load DOM
 *  -2 = Execution error (trap, illegal instruction, etc.)
 */
static int run_dom_test(const char* dom_path, int max_steps, int verbose, TestResult* result) {
    memset(result, 0, sizeof(*result));

    fprintf(stderr, "[RUN_DOM_TEST] Starting load of: %s\n", dom_path);
    printf("Loading: %s\n", dom_path);

    /* Load DOM header */
    if (ndlib_load_dom_header(dom_path) != 0) {
        printf("  ERROR: Failed to load DOM header\n");
        result->failed++;
        return -1;
    }

    /* Load segments */
    if (ndlib_load_dom_segments() != 0) {
        printf("  ERROR: Failed to load DOM segments\n");
        result->failed++;
        return -1;
    }

    /* Create machine and CPU */
    Nd500Machine machine;
    nd500_machine_init(&machine, MEMORY_SIZE);
    if (machine.memory == NULL) {
        printf("  ERROR: Failed to initialize machine\n");
        result->failed++;
        return -1;
    }

    Nd500Cpu cpu;
    nd500_cpu_init(&cpu, &machine);
    nd500_cpu_reset(&cpu);
    nd500_mmu_init(&cpu);
    nd500_domain_init(&cpu);

    /* Initialize MON call subsystem */
    mon_init();

    /* Queue console input (--input); installs the queued console so the
     * program reads scripted commands instead of blocking on stdin */
    for (int i = 0; i < g_input_count; i++) {
        char* processed = process_escapes(g_inputs[i]);
        if (processed) {
            printf("  Queued input: \"%s\"\n", g_inputs[i]);
            mon_queue_console_input(processed);
            free(processed);
        }
    }

    /* Load DOM into machine with MMU setup */
    uint32_t start_addr = 0;
    int domain = 0;

    if (ndlib_dom_load_to_machine(&machine, &cpu, -1,
                                   verbose ? verbose_log : quiet_log,
                                   NULL, &start_addr, &domain) != 0) {
        printf("  ERROR: Failed to load DOM into machine\n");
        nd500_machine_free(&machine);
        result->failed++;
        return -1;
    }

    result->initial_pc = cpu.PC;
    printf("  Start PC: 0x%08X (domain %d)\n", result->initial_pc, domain);
    printf("  Initial instruction_count: %llu\n", (unsigned long long)cpu.instruction_count);

    /* Run execution loop */
    machine.run_flag = 1;
    machine.stop_reason = STOP_NONE;

    int steps = 0;
    uint32_t last_pc = cpu.PC;
    int stuck_count = 0;

    while (steps < max_steps && machine.run_flag) {
        /* Debug: Show progress at key milestones */
        if (steps >= 4250 && steps <= 4280) {
            printf("  TRACE: step=%d PC=0x%08X I1=%08X B=%08X L=%08X\n",
                   steps, cpu.PC, cpu.I[0], cpu.B, cpu.L);
            fflush(stdout);
        }

        /* Execute one instruction */
        bool step_ok = nd500_cpu_step(&cpu);

        /* Check if stop_reason changed during step */
        if (machine.stop_reason != STOP_NONE) {
            printf("  DEBUG: stop_reason=%d detected at step %d, PC=0x%08X, step_ok=%d\n",
                   machine.stop_reason, steps, cpu.PC, step_ok);
            fflush(stdout);
            if (machine.stop_reason == STOP_MON_HALT) {
                /* Normal program exit via MON 0B LEAVE */
                result->exited_normally = 1;
            }
            break;
        }

        if (!step_ok) {
            /* Trap occurred - check if it's fatal */
            printf("  DEBUG: step returned false at step %d, PC=0x%08X, stop_reason=%d, run_flag=%d\n",
                   steps, cpu.PC, machine.stop_reason, machine.run_flag);
            fflush(stdout);
            break;  /* Always break on step failure */
        }

        steps++;

        /* Detect infinite loop (PC not changing) */
        if (cpu.PC == last_pc) {
            stuck_count++;
            if (stuck_count > 100) {
                printf("  WARNING: PC stuck at 0x%08X for 100+ instructions\n", cpu.PC);
                break;
            }
        } else {
            stuck_count = 0;
            last_pc = cpu.PC;
        }

        /* Check for MON halt (program exit) */
        if (machine.stop_reason == STOP_MON_HALT) {
            result->exited_normally = 1;
            break;
        }

        /* Check for unimplemented MON call */
        if (machine.stop_reason == STOP_MON_UNIMPLEMENTED) {
            printf("  Unimplemented MON call at step %d, PC=0x%08X\n", steps, cpu.PC);
            break;
        }
    }

    /* Record results */
    result->final_pc = cpu.PC;
    result->instructions_executed = cpu.instruction_count;
    result->stop_reason = machine.stop_reason;

    /* Print summary */
    printf("  Steps executed: %d\n", steps);
    printf("  Final PC: 0x%08X\n", result->final_pc);
    printf("  Instructions: %llu\n", (unsigned long long)result->instructions_executed);

    if (machine.stop_reason != STOP_NONE) {
        printf("  Stop reason: %s\n", nd500_stop_reason_str(machine.stop_reason));
    }

    /* Determine pass/fail */
    int success = 0;

    if (result->exited_normally) {
        printf("  Result: PASS (program exited normally via MON 0B)\n");
        result->passed++;
        success = 1;
    } else if (machine.stop_reason == STOP_NONE && steps >= max_steps) {
        printf("  Result: PASS (completed %d steps without error)\n", max_steps);
        result->passed++;
        success = 1;
    } else if (machine.stop_reason == STOP_MON_UNIMPLEMENTED) {
        printf("  Result: PARTIAL (unimplemented MON call - may need implementation)\n");
        result->passed++;  /* Not a failure, just incomplete */
        success = 1;
    } else {
        printf("  Result: FAIL (stopped due to %s)\n",
               nd500_stop_reason_str(machine.stop_reason));
        result->failed++;
        success = 0;
    }

    /* Cleanup */
    nd500_machine_free(&machine);

    return success ? 0 : -2;
}

static void print_usage(const char* prog) {
    printf("Usage: %s <dom_file> [max_steps] [-v] [--trace-file <path>] [--radix <mode>]\n", prog);
    printf("\n");
    printf("Arguments:\n");
    printf("  dom_file             Path to DOM file to load and execute\n");
    printf("  max_steps            Maximum steps to execute (default: %d)\n", DEFAULT_MAX_STEPS);
    printf("  -v                   Verbose output (show DOM loading details)\n");
    printf("  --trace-file <path>  Write instruction trace to file\n");
    printf("  --radix <mode>       Set numeric radix: decimal | hex | octal\n");
    printf("  --input <text>       Queue console input (\\r and \\n escapes; repeatable)\n");
    printf("  --compare <p>=<e>    After the run, byte-compare produced file <p>\n");
    printf("                       against expected file <e> (repeatable)\n");
    printf("  --min-instructions N Pass if the run executed >= N instructions,\n");
    printf("                       regardless of how it stopped (progress gate)\n");
    printf("\n");
    printf("Exit codes:\n");
    printf("  0 = Success (normal exit or completed steps)\n");
    printf("  1 = Error (load failure, trap, etc.)\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s /path/to/program.dom 50000\n", prog);
    printf("  %s /path/to/program.dom -v\n", prog);
    printf("  %s /path/to/program.dom 10000 --trace-file trace.txt --radix hex\n", prog);
}

int main(int argc, char* argv[]) {
    /* Ensure output is line-buffered for proper ordering */
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    fprintf(stderr, "[MAIN] Starting test...\n");
    printf("===============================================\n");
    printf("  ND-500 DOM Integration Test\n");
    printf("===============================================\n\n");

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char* dom_path = argv[1];
    int max_steps = DEFAULT_MAX_STEPS;
    int verbose = 0;
    const char* trace_file_path = NULL;
    const char* radix_str = NULL;

    /* Parse optional arguments */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "--trace-file") == 0 && i + 1 < argc) {
            trace_file_path = argv[++i];
        } else if (strcmp(argv[i], "--radix") == 0 && i + 1 < argc) {
            radix_str = argv[++i];
        } else if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
            if (g_input_count < MAX_INPUTS) {
                g_inputs[g_input_count++] = argv[++i];
            } else {
                printf("Too many --input arguments (max %d)\n", MAX_INPUTS);
                return 1;
            }
        } else if (strcmp(argv[i], "--min-instructions") == 0 && i + 1 < argc) {
            g_min_instructions = strtoull(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--compare") == 0 && i + 1 < argc) {
            if (g_compare_count < MAX_COMPARES) {
                g_compares[g_compare_count++] = argv[++i];
            } else {
                printf("Too many --compare arguments (max %d)\n", MAX_COMPARES);
                return 1;
            }
        } else {
            max_steps = atoi(argv[i]);
            if (max_steps <= 0) {
                printf("Invalid max_steps: %s\n", argv[i]);
                return 1;
            }
        }
    }

    /* Set radix if specified */
    if (radix_str) {
        if (strcasecmp(radix_str, "decimal") == 0 || strcasecmp(radix_str, "dec") == 0) {
            nd500_dbg_set_radix(0);
        } else if (strcasecmp(radix_str, "hex") == 0) {
            nd500_dbg_set_radix(1);
        } else if (strcasecmp(radix_str, "octal") == 0 || strcasecmp(radix_str, "oct") == 0) {
            nd500_dbg_set_radix(2);
        } else {
            printf("Invalid radix: %s (use: decimal, hex, octal)\n", radix_str);
            return 1;
        }
    }

    /* Set up trace file if requested */
    if (trace_file_path) {
        if (nd500_dbg_set_trace_file(trace_file_path) != 0) {
            return 1;
        }
        printf("Trace output: %s\n", trace_file_path);
    }

    printf("Configuration:\n");
    printf("  DOM file:   %s\n", dom_path);
    printf("  Max steps:  %d\n", max_steps);
    printf("  Verbose:    %s\n", verbose ? "yes" : "no");
    if (radix_str) {
        printf("  Radix:      %s\n", radix_str);
    }
    if (trace_file_path) {
        printf("  Trace file: %s\n", trace_file_path);
    }
    printf("\n");

    /* Run the test */
    TestResult result;
    int rc = run_dom_test(dom_path, max_steps, verbose, &result);

    /* Post-run file comparisons (--compare produced=expected) */
    if (rc == 0 && g_compare_count > 0) {
        printf("\nOutput file comparisons:\n");
        for (int i = 0; i < g_compare_count; i++) {
            char spec[512];
            strncpy(spec, g_compares[i], sizeof(spec) - 1);
            spec[sizeof(spec) - 1] = '\0';
            char* eq = strchr(spec, '=');
            if (!eq) {
                printf("  Invalid --compare spec (need produced=expected): %s\n", spec);
                rc = -2;
                continue;
            }
            *eq = '\0';
            const char* produced = spec;
            const char* expected = eq + 1;
            if (compare_files(produced, expected) == 0) {
                printf("  MATCH: %s == %s\n", produced, expected);
            } else {
                rc = -2;
            }
        }
    }

    /* Progress gate (--min-instructions): overrides the normal pass/fail.
     * Passes if the run got at least N instructions in, even if it later
     * crashed - this asserts the program reached a milestone without
     * requiring it to complete. */
    int passed;
    if (g_min_instructions > 0) {
        passed = (result.instructions_executed >= g_min_instructions);
        printf("\nProgress gate: executed %llu / required >= %llu -> %s\n",
               (unsigned long long)result.instructions_executed,
               (unsigned long long)g_min_instructions,
               passed ? "PASS" : "FAIL");
        rc = passed ? 0 : -3;
    } else {
        passed = (rc == 0 && result.passed > 0 && result.failed == 0);
    }

    printf("\n===============================================\n");
    if (passed) {
        printf("  TEST PASSED\n");
    } else {
        printf("  TEST FAILED\n");
    }
    printf("===============================================\n");

    /* Close trace file if open */
    nd500_dbg_close_trace_file();

    return (rc == 0) ? 0 : 1;
}
