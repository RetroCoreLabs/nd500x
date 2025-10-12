#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../machine/machine_protos.h"
#include "../../cpu/cpu_protos.h"
#include "../../debugger/debugger.h"
#include "../../ndlib/ndlib.h"

int main(int argc, char** argv) {
	int debug = 0;
	const char* input_path = NULL;
    uint32_t dis_len = 0;
    uint32_t dis_addr = 0;
    uint32_t hex_len = 0;
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--debug") == 0) {
			debug = 1;
		} else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
			input_path = argv[++i];
        } else if (strcmp(argv[i], "--disasm") == 0 && i + 1 < argc) {
            dis_len = (uint32_t)strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--addr") == 0 && i + 1 < argc) {
            dis_addr = (uint32_t)strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--hexdump") == 0 && i + 1 < argc) {
            hex_len = (uint32_t)strtoul(argv[++i], NULL, 0);
		}
	}

	Nd500Machine machine;
	nd500_machine_init(&machine, 8 * 1024 * 1024);
	Nd500Cpu cpu;
	nd500_cpu_init(&cpu, &machine);
	nd500_cpu_reset(&cpu);

	uint32_t text_size = 0;
	if (input_path) {
		uint32_t entry = 0;
		if (ndlib_loadaout_file_ex(&machine, input_path, &entry, &text_size) == 0) {
			printf("loaded, entry=0x%08X\n", entry);
			(void)ndlib_symbols_load(input_path);
			(void)ndlib_aout_dump_metadata(input_path);
			cpu.PC = entry;
		} else {
			printf("load failed: %s\n", input_path);
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


