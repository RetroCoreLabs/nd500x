/*
 * Diagnostic: dump guest memory at a virtual address when a PC is reached.
 *
 * Motivating question: MON 50B OPEN logs "desc=[len=17, ptr=0xB0001DE8]" but
 * then "result len=0 str=''". Either the name buffer really is empty (the
 * linker's own state) or our descriptor reader is dropping it (our bug). Only
 * the bytes at the pointer can decide, and they must be read AT the call site -
 * before or after, the frame may hold something else.
 *
 * Build (make does NOT build this - hand-link against the static libs):
 *   gcc -O2 -o build/bin/diag_dumpat test/diag_dumpat.c \
 *     -Wl,--start-group build/lib/libnd500_debugger.a build/lib/libnd500_cpu.a \
 *     build/lib/libnd500_machine.a build/lib/libmon.a build/lib/libnd500_ndlib.a \
 *     build/lib/libnd500_disasm.a -Wl,--end-group -lpthread -lm
 *
 * Run:
 *   ./build/bin/diag_dumpat <DOM> <stop_pc> <maxsteps> <va> <len> [va len ...]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/cpu/nd500_domain.h"
#include "../src/ndlib/ndlib.h"
#include "../src/libmon/mon.h"
#include "../src/libmon/mon_file_table.h"

#define MEMSZ (16u*1024u*1024u)

static uint32_t xlate(Nd500Cpu* c, uint32_t va) {
    if (c->machine && c->machine->mmu_enabled) return nd500_mmu_translate(c, va, 0, 0);
    return va;
}

static void dump(Nd500Cpu* c, Nd500Machine* m, uint32_t va, uint32_t len) {
    printf("  %08X: ", va);
    char ascii[17]; uint32_t col = 0;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t pa = xlate(c, va + i);
        if (pa == 0xFFFFFFFFu || pa >= MEMSZ) { printf(".. "); ascii[col] = '?'; }
        else {
            uint8_t b = nd500_bus_read8(m, pa);
            printf("%02X ", b);
            ascii[col] = isprint(b) ? (char)b : '.';
        }
        col++;
        if (col == 16) {
            ascii[16] = 0; printf(" |%s|\n", ascii);
            if (i + 1 < len) printf("  %08X: ", va + i + 1);
            col = 0;
        }
    }
    if (col) {
        for (uint32_t k = col; k < 16; k++) printf("   ");
        ascii[col] = 0; printf(" |%s|\n", ascii);
    }
}

int main(int argc, char** argv) {
    if (argc < 6) {
        fprintf(stderr, "usage: %s <DOM> <stop_pc> <maxsteps> <va> <len> [va len ...]\n", argv[0]);
        return 2;
    }
    const char* dom = argv[1];
    uint32_t stop_pc = (uint32_t)strtoul(argv[2], 0, 0);
    long maxsteps = atol(argv[3]);

    if (ndlib_load_dom_header(dom) || ndlib_load_dom_segments()) {
        fprintf(stderr, "load fail\n"); return 2;
    }
    Nd500Machine m; Nd500Cpu c;
    nd500_machine_init(&m, MEMSZ); nd500_cpu_init(&c, &m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    mon_log_enable(0);

    uint32_t sa = 0; int dm = 0;
    if (ndlib_dom_load_to_machine(&m, &c, -1, NULL, NULL, &sa, &dm)) {
        fprintf(stderr, "load2 fail\n"); return 2;
    }

    const char* cmd = getenv("ND500X_DUMPAT_CMD");
    if (cmd && *cmd) {
        char line[160];
        snprintf(line, sizeof(line), "%s\r", cmd);
        mon_set_command_buffer(line);
        mon_queue_console_input(line);
    }

    m.run_flag = 1; m.stop_reason = STOP_NONE;
    long n = 0; int hit = 0;
    for (; n < maxsteps && m.run_flag; n++) {
        if (c.PC == stop_pc) {
            hit = 1;
            printf("HIT PC=%08X at instr=%llu  B=%08X\n", c.PC,
                   (unsigned long long)c.instruction_count, c.B);
            for (int i = 4; i + 1 < argc; i += 2) {
                uint32_t va = (uint32_t)strtoul(argv[i], 0, 0);
                uint32_t len = (uint32_t)strtoul(argv[i+1], 0, 0);
                printf("--- %s len=%u ---\n", argv[i], len);
                dump(&c, &m, va, len);
            }
            break;
        }
        nd500_cpu_step(&c);
    }
    if (!hit) printf("PC %08X never reached in %ld instr\n", stop_pc, n);
    nd500_machine_free(&m);
    return 0;
}
