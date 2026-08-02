/*
 * Diagnostic: NC per-instruction PC trace over a window, for the cross-emulator
 * first-divergence diff. Emits ONE row per executed instruction in [start,end]:
 *
 *     instr#   PC        B         I1        I2      (hex, no 0x)
 *
 * Format locked with the RetroCore (C#) csharp-first-window-pc-trace.txt so the
 * two files line-diff directly by instruction number.
 *
 * Run PINNED (ND500X_PIN_CLOCK=1) from build/nc_sandbox. Fixture (source file and
 * NC command) is passed on argv so both emulators can use a byte-identical setup.
 *
 * argv: <dom> <cmd> <max_steps> <out> <win_start> <win_end>
 *   <cmd> may contain multiple NC commands separated by the literal token ";;"
 *         (each gets a trailing CR). Use \r for an explicit CR inside a command.
 *
 * Build (same static-lib link line as diag_nc_checkpoints):
 *   gcc -O2 -o build/bin/diag_nc_pctrace test/diag_nc_pctrace.c \
 *     -Wl,--start-group build/lib/libnd500_debugger.a build/lib/libnd500_cpu.a \
 *     build/lib/libnd500_machine.a build/lib/libmon.a build/lib/libnd500_ndlib.a \
 *     build/lib/libnd500_disasm.a -Wl,--end-group -lpthread -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/cpu/nd500_domain.h"
#include "../src/ndlib/ndlib.h"
#include <ndmon/mon.h>
#include <ndmon/mon_file_table.h>
#include <ndmon/mon_clock.h>
#include "testdata.h"

#define MEMORY_SIZE   (16u * 1024u * 1024u)

/* Expand a command spec into the console input buffer:
 *   ";;" -> command separator (emit CR)
 *   "\r" -> explicit CR
 * Every command implicitly ends with CR at a ";;" boundary; a trailing CR is
 * also appended for the last command. */
static void build_input(const char* spec, char* out, size_t outsz) {
    size_t j = 0;
    for (size_t i = 0; spec[i] && j < outsz - 2; i++) {
        if (spec[i] == ';' && spec[i + 1] == ';') { out[j++] = '\r'; i++; }
        else if (spec[i] == '\\' && spec[i + 1] == 'r') { out[j++] = '\r'; i++; }
        else out[j++] = spec[i];
    }
    if (j == 0 || out[j - 1] != '\r') out[j++] = '\r';
    out[j] = 0;
}

int main(int argc, char** argv) {
    const char* dom = (argc > 1) ? argv[1] : nd500_testdata("FraTor/nc/nc-a06.dom");
    const char* cmd = (argc > 2) ? argv[2] : "COMPILE A,A,A";
    long max_steps  = (argc > 3) ? strtol(argv[3], NULL, 0) : 6001L;
    const char* out = (argc > 4) ? argv[4] : NULL;
    unsigned long long win_start = (argc > 5) ? strtoull(argv[5], NULL, 0) : 0ull;
    unsigned long long win_end   = (argc > 6) ? strtoull(argv[6], NULL, 0) : 6000ull;

    FILE* fp = stdout;
    if (out) { fp = fopen(out, "w"); if (!fp) { fprintf(stderr, "open %s failed\n", out); return 2; } }

    if (ndlib_load_dom_header(dom) != 0) { fprintf(stderr, "header load failed\n"); return 2; }
    if (ndlib_load_dom_segments() != 0)  { fprintf(stderr, "segment load failed\n"); return 2; }

    Nd500Machine machine;
    nd500_machine_init(&machine, MEMORY_SIZE);
    if (!machine.memory) { fprintf(stderr, "machine init failed\n"); return 2; }

    Nd500Cpu cpu;
    nd500_cpu_init(&cpu, &machine);
    nd500_cpu_reset(&cpu);
    nd500_mmu_init(&cpu);
    nd500_domain_init(&cpu);
    mon_init();

    char inbuf[128];
    build_input(cmd, inbuf, sizeof(inbuf));
    mon_queue_console_input(inbuf);

    uint32_t start_addr = 0; int domain = 0;
    if (ndlib_dom_load_to_machine(&machine, &cpu, -1, NULL, NULL,
                                  &start_addr, &domain) != 0) {
        fprintf(stderr, "dom load-to-machine failed\n"); return 2;
    }
    fprintf(stderr, "PC trace: start PC=0x%08X window=[%llu,%llu] clock=%s\n",
            cpu.PC, win_start, win_end,
            mon_clock_is_deterministic() ? "PINNED" : "REAL(non-det!)");

    fprintf(fp, "# instr#   PC        B         I1        I2\n");

    machine.run_flag = 1;
    machine.stop_reason = STOP_NONE;

    long step = 0;
    for (; step < max_steps && machine.run_flag; step++) {
        unsigned long long ic = cpu.instruction_count;
        if (ic >= win_start && ic <= win_end) {
            fprintf(fp, "%8llu   %08X  %08X  %08X  %08X\n",
                    ic, cpu.PC, cpu.B, cpu.I[0], cpu.I[1]);
        }
        if (ic > win_end) break;

        nd500_cpu_step(&cpu);
        if (machine.run_flag == 0 && machine.stop_reason != STOP_NONE) {
            fprintf(fp, "# STOP=%s at instr=%llu PC=0x%08X\n",
                    nd500_stop_reason_str(machine.stop_reason),
                    (unsigned long long)cpu.instruction_count, cpu.PC);
            break;
        }
    }

    fprintf(stderr, "Done. steps=%ld last_PC=0x%08X stop=%s\n",
            step, cpu.PC, nd500_stop_reason_str(machine.stop_reason));
    if (fp != stdout) fclose(fp);
    nd500_machine_free(&machine);
    return 0;
}
