#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "../../machine/machine_protos.h"
#include "../../cpu/cpu_protos.h"
#include "../../cpu/nd500_mmu.h"
#include "../../cpu/nd500_domain.h"
#include "../../debugger/debugger.h"
#include "../../debugger/commands.h"
#include "../../ndlib/ndlib.h"
#include "../../ndlib/ndlib_color.h"
#include "../../libmon/mon.h"
#include "nd500_dom.h"

static void print_usage(const char* prog) {
    printf("ND-500 Emulator - nd500x\n\n");
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  --debug                  Enter interactive debugger REPL\n");
    printf("  -i <path>                Load a.out file (legacy)\n");
    printf("  --aout <path>            Load a.out file\n");
    printf("  --pseg <path>            Load PSEG binary (auto-detects kernel/user mode)\n");
    printf("  --dseg <path>            Load DSEG binary (auto-detects kernel/user mode)\n");
    printf("  --dom <path>             Load DOM/SEG file header\n");
    printf("  --mode <mode>            Override mode: kernel | user | auto\n");
    printf("                           (kernel: PSEG=0x08000000, DSEG=0x00000000)\n");
    printf("                           (user:   PSEG=0xD0000000, DSEG=0xF0000000)\n");
    printf("                           (auto:   detect from filename - 'user' in name)\n");
    printf("  --pc <addr>              Set starting PC address\n");
    printf("  --disasm <len>           Disassemble <len> bytes and exit\n");
    printf("  --addr <addr>            Start address for disassembly (default: 0)\n");
    printf("  --hexdump <len>          Hex dump <len> bytes and exit\n");
    printf("  --radix <mode>           Set numeric radix: decimal | hex | octal\n");
    printf("  -ansi                    Force enable ANSI colors\n");
    printf("  -noansi                  Force disable ANSI colors\n");
    printf("  --help                   Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --aout kernel --debug\n", prog);
    printf("  %s --pseg kernel.pseg --dseg kernel.dseg --mode kernel --debug\n", prog);
    printf("  %s --pseg user_prog.pseg --mode user --debug\n", prog);
    printf("  %s --aout program --disasm 100 --addr 0x1000\n", prog);
    printf("\n");
}

int main(int argc, char** argv) {
    int debug = 0;
    const char* input_path = NULL;
    const char* aout_path = NULL;
    const char* pseg_path = NULL;
    const char* dseg_path = NULL;
    const char* dom_path = NULL;
    const char* mode_str = NULL; /* "kernel" or "user" */
    uint32_t start_pc = 0;
    int has_start_pc = 0;
    uint32_t dis_len = 0;
    uint32_t dis_addr = 0;
    uint32_t hex_len = 0;
    int ansi_flag = 0; /* 0=auto, 1=force-enable, -1=force-disable */
    const char* radix_str = NULL;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--debug") == 0) {
            debug = 1;
        } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            input_path = argv[++i];
        } else if (strcmp(argv[i], "--aout") == 0 && i + 1 < argc) {
            aout_path = argv[++i];
        } else if (strcmp(argv[i], "--pseg") == 0 && i + 1 < argc) {
            pseg_path = argv[++i];
        } else if (strcmp(argv[i], "--dseg") == 0 && i + 1 < argc) {
            dseg_path = argv[++i];
        } else if (strcmp(argv[i], "--dom") == 0 && i + 1 < argc) {
            dom_path = argv[++i];
        } else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            mode_str = argv[++i];
        } else if (strcmp(argv[i], "--pc") == 0 && i + 1 < argc) {
            start_pc = (uint32_t)strtoul(argv[++i], NULL, 0);
            has_start_pc = 1;
        } else if (strcmp(argv[i], "--disasm") == 0 && i + 1 < argc) {
            dis_len = (uint32_t)strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--addr") == 0 && i + 1 < argc) {
            dis_addr = (uint32_t)strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--hexdump") == 0 && i + 1 < argc) {
            hex_len = (uint32_t)strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--radix") == 0 && i + 1 < argc) {
            radix_str = argv[++i];
        } else if (strcmp(argv[i], "-ansi") == 0) {
            ansi_flag = 1;
        } else if (strcmp(argv[i], "-noansi") == 0) {
            ansi_flag = -1;
        }
    }
    
    /* Initialize color system based on flags */
    ndlib_color_init(ansi_flag);

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

    Nd500Machine machine;
    /* Initialize with 16MB of physical memory
     * Virtual addresses are mapped to physical via MMU */
    nd500_machine_init(&machine, 16 * 1024 * 1024);
	Nd500Cpu cpu;
	nd500_cpu_init(&cpu, &machine);
	nd500_cpu_reset(&cpu);

	/* Initialize SINTRAN MON call emulation */
	mon_init();

	uint32_t text_size = 0;
    if (input_path) {
		uint32_t entry = 0, pc = 0;
		/* Use unified loading (auto-loads .map and .s files) */
		if (ndlib_load_aout_with_debug(&machine, input_path, 1, &entry, &pc) == 0) {
			printf("loaded: %s (entry=0x%08X, PC=0x%08X)\n", input_path, entry, pc);
			(void)ndlib_aout_dump_metadata(input_path);
		} else {
			printf("load failed: %s\n", input_path);
		}
	}

    if (aout_path) {
        /* Look for initialization script BEFORE loading the aout file
         * This allows the script to configure MMU before data is loaded */
        char init_path[512];
        strncpy(init_path, aout_path, sizeof(init_path) - 1);
        init_path[sizeof(init_path) - 1] = '\0';

        /* Find extension or end of string */
        char* ext = strrchr(init_path, '.');
        if (ext && (strcmp(ext, ".o") == 0 || strcmp(ext, ".out") == 0)) {
            /* Replace .o or .out with .init */
            strcpy(ext, ".init");
        } else {
            /* No extension - append .init to basename (e.g., kernel → kernel.init) */
            strncat(init_path, ".init", sizeof(init_path) - strlen(init_path) - 1);
        }

        /* Execute init script BEFORE loading aout (ignore errors - script is optional) */
        nd500_execute_init_script(&machine, init_path);

        /* Now load the aout file with MMU potentially configured */
        uint32_t entry = 0, pc = 0;
		/* Use unified loading (auto-loads .map and .s files) */
        if (ndlib_load_aout_with_debug(&machine, aout_path, 1, &entry, &pc) == 0) {
            printf("loaded: %s (entry=0x%08X, PC=0x%08X)\n", aout_path, entry, pc);
            (void)ndlib_aout_dump_metadata(aout_path);
            if (has_start_pc) cpu.PC = start_pc;  /* Override PC if specified */
        } else {
            printf("load failed: %s\n", aout_path);
        }
    }

    if (pseg_path || dseg_path) {
        int kernel_mode = 1;
        int mode_specified = 0;

        if (mode_str) {
            if (strcasecmp(mode_str, "kernel") == 0) {
                kernel_mode = 1;
                mode_specified = 1;
            } else if (strcasecmp(mode_str, "user") == 0) {
                kernel_mode = 0;
                mode_specified = 1;
            } else if (strcasecmp(mode_str, "auto") == 0) {
                /* Auto-detect based on filename */
                mode_specified = 1;
                if (pseg_path && strstr(pseg_path, "user")) kernel_mode = 0;
                else if (dseg_path && strstr(dseg_path, "user")) kernel_mode = 0;
                else kernel_mode = 1;
            }
        }

        /* Auto-detect if not specified */
        if (!mode_specified) {
            /* Default: kernel mode unless filename contains "user" */
            if (pseg_path && strstr(pseg_path, "user")) kernel_mode = 0;
            else if (dseg_path && strstr(dseg_path, "user")) kernel_mode = 0;
            else kernel_mode = 1;
        }

        const char* mode_name = kernel_mode ? "kernel" : "user";
        uint32_t pseg_base = kernel_mode ? 0x08000000u : 0xD0000000u;
        uint32_t dseg_base = kernel_mode ? 0x00000000u : 0xF0000000u;

        if (pseg_path) {
            int rc = nd500_load_pseg_file(&machine, pseg_path, pseg_base);
            if (rc == 0) {
                printf("PSEG loaded at 0x%08X from %s (%s mode)\n", pseg_base, pseg_path, mode_name);
            } else {
                printf("PSEG load failed: %s\n", pseg_path);
                printf("Error: %s\n", nd500_load_strerror(rc, pseg_base, machine.memory_size));
            }
        }
        if (dseg_path) {
            int rc = nd500_load_dseg_file(&machine, dseg_path, dseg_base);
            if (rc == 0) {
                printf("DSEG loaded at 0x%08X from %s (%s mode)\n", dseg_base, dseg_path, mode_name);
            } else {
                printf("DSEG load failed: %s\n", dseg_path);
                printf("Error: %s\n", nd500_load_strerror(rc, dseg_base, machine.memory_size));
            }
        }
        if (has_start_pc) cpu.PC = start_pc; else if (pseg_path) cpu.PC = pseg_base;
    }

    if (dom_path) {
        int rc = ndlib_load_dom_header(dom_path);
        if (rc != 0) {
            if (rc == -1) {
                printf("DOM load failed: %s (file not found)\n", dom_path);
            } else if (rc == -2) {
                printf("DOM load failed: %s (file too small?)\n", dom_path);
            } else {
                printf("DOM load failed: %s (error %d)\n", dom_path, rc);
            }
        } else {
            /* Load segments into memory */
            rc = ndlib_load_dom_segments();
            if (rc != 0) {
                printf("DOM segment load failed: %s\n", dom_path);
            } else {
                /* Load segments to machine and configure MMU/domain system */
                uint32_t start_addr = 0;
                rc = ndlib_dom_load_to_machine(&machine, &cpu, ndlib_dom_log_printf, NULL, &start_addr);
                if (rc != 0) {
                    printf("DOM configuration failed: %s\n", dom_path);
                } else {
                    /* Override PC if user specified --pc flag */
                    if (has_start_pc) {
                        cpu.PC = start_pc;
                    }
                    printf("DOM loaded: %s (start=0x%08X, MMU enabled)\n", dom_path, cpu.PC);
                }
            }
        }
    }

	if (debug) {
		return nd500_debugger_repl(&machine);
	}

    if (hex_len > 0) {
        uint32_t end = dis_addr + hex_len;
        for (uint32_t a = dis_addr; a < end; ++a) {
            if ((a - dis_addr) % 16 == 0) printf("%08X: ", a);
            printf("%02X ", nd500_bus_read8(&machine, a));
            if ((a - dis_addr) % 16 == 15 || a + 1 == end) printf("\n");
        }
        return 0;
    }

    if (dis_len > 0) {
        /* If text_size is known and user didn't specify length, clamp to text */
        uint32_t actual_len = (text_size > 0 && dis_len > text_size) ? text_size : dis_len;
        char outbuf[16384];
        size_t n = nd500_dbg_disasm(&machine, dis_addr, actual_len, outbuf, sizeof(outbuf));
        if (n > 0) {
            fwrite(outbuf, 1, n, stdout);
            if (outbuf[n-1] != '\n') fputc('\n', stdout);
        }
        return 0;
    }

	printf("nd500x running (no UI). Use --debug for REPL.\n");
	return 0;
}


