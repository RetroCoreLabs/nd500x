/*
 * Diagnostic: find WHO writes the linker's DVOUTS output buffer.
 *
 * The linker calls MON 504B DVOUTS(dev=1, n=233, buf=0xB0049430). That buffer
 * is all-zero in the DOM (BSS/scratch) and gets filled at RUNTIME with a mix of
 * real prompt text AND ND-500 pointers / NULs - which then print as garbage on
 * the console. This harness watches the first WINDOW bytes of the buffer and, on
 * every byte that changes, logs (instr#, the PC that just executed, offset,
 * old->new). That names the exact instruction(s) building the buffer, deciding
 * "linker composes it wrong" vs "a CPU instruction mis-copies it".
 *
 * Build (same static-lib link line as the other diag harnesses):
 *   gcc -O2 -o build/bin/diag_dvbuf test/diag_dvbuf.c \
 *     -Wl,--start-group build/lib/libnd500_debugger.a build/lib/libnd500_cpu.a \
 *     build/lib/libnd500_machine.a build/lib/libmon.a build/lib/libnd500_ndlib.a \
 *     build/lib/libnd500_disasm.a -Wl,--end-group -lpthread -lm
 *
 * Run:
 *   ./build/bin/diag_dvbuf [DOM] [WATCH_VA] [WINDOW] [maxsteps]
 *   default: linker-b01.dom  0xB0049430  64  400000
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

#define MEMSZ (16u*1024u*1024u)
#define WINMAX 256

static uint32_t xlate(Nd500Cpu* c, uint32_t va) {
    if (c->machine && c->machine->mmu_enabled) return nd500_mmu_translate(c, va, 0, 0);
    return va;
}

int main(int argc, char** argv) {
    const char* dom = (argc > 1) ? argv[1] : nd500_testdata("nd-linker/linker-b01.dom");
    uint32_t WATCH   = (argc > 2) ? (uint32_t)strtoul(argv[2], 0, 0) : 0xB0049430u;
    uint32_t WINDOW  = (argc > 3) ? (uint32_t)strtoul(argv[3], 0, 0) : 64u;
    long maxsteps    = (argc > 4) ? atol(argv[4]) : 400000L;
    if (WINDOW > WINMAX) WINDOW = WINMAX;

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
    printf("Watching VA 0x%08X..0x%08X for writes; entry=0x%08X\n",
           WATCH, WATCH + WINDOW - 1, c.PC);

    unsigned char prev[WINMAX];
    int prev_valid = 0;
    memset(prev, 0, sizeof(prev));

    m.run_flag = 1; m.stop_reason = STOP_NONE;
    long step = 0;
    int changes = 0;
    for (; step < maxsteps && m.run_flag; step++) {
        uint32_t pc_before = c.PC;
        nd500_cpu_step(&c);

        /* Read the window (best-effort; skip if not currently mapped). */
        unsigned char cur[WINMAX];
        int ok = 1;
        for (uint32_t i = 0; i < WINDOW; i++) {
            uint32_t pa = xlate(&c, WATCH + i);
            if (pa == 0xFFFFFFFFu || pa + 1 > MEMSZ) { ok = 0; break; }
            cur[i] = nd500_bus_read8(&m, pa);
        }
        if (!ok) { if (m.run_flag == 0 && m.stop_reason != STOP_NONE) break; continue; }

        if (prev_valid) {
            for (uint32_t i = 0; i < WINDOW; i++) {
                if (cur[i] != prev[i]) {
                    printf("  #%llu PC=%08X  buf[+%02u] %02X -> %02X  (%c->%c)\n",
                           (unsigned long long)c.instruction_count, pc_before, i,
                           prev[i], cur[i],
                           (prev[i] >= 0x20 && prev[i] < 0x7F) ? prev[i] : '.',
                           (cur[i]  >= 0x20 && cur[i]  < 0x7F) ? cur[i]  : '.');
                    if (++changes >= 120) { printf("  [reached 120 changes, stopping]\n"); goto done; }
                }
            }
        }
        memcpy(prev, cur, WINDOW);
        prev_valid = 1;

        if (m.run_flag == 0 && m.stop_reason != STOP_NONE) break;
    }
done:
    printf("stop=%s instr#=%llu PC=%08X steps=%ld changes=%d\n",
           nd500_stop_reason_str(m.stop_reason),
           (unsigned long long)c.instruction_count, c.PC, step, changes);
    nd500_machine_free(&m);
    return 0;
}
