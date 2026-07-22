/*
 * Diagnostic: read the linker's copy-loop bounds each time its routine is entered.
 *
 * Static reading of B0047150 shows:
 *   b.44  := b.24                 (loop index start)
 *   b.152 := b.24 + b.32 - 1      (loop limit)
 *   ... loopi b.44,b.152
 * so b.24 is a START index and b.32 a COUNT, both frame locals (B-relative).
 *
 * The linker cycles this region forever with no I/O. This harness stops at a
 * given PC and prints the frame values, so we can see whether the SAME bounds
 * repeat every pass (an outer loop that never advances) or whether they change
 * (real work with a bound that never terminates).
 *
 * Build (make does NOT build this - hand-link against the static libs):
 *   gcc -O2 -o build/bin/diag_loopargs test/diag_loopargs.c \
 *     -Wl,--start-group build/lib/libnd500_debugger.a build/lib/libnd500_cpu.a \
 *     build/lib/libnd500_machine.a build/lib/libmon.a build/lib/libnd500_ndlib.a \
 *     build/lib/libnd500_disasm.a -Wl,--end-group -lpthread -lm
 *
 * Run:
 *   ./build/bin/diag_loopargs [DOM] [PC] [maxsteps] [maxhits]
 *   default: linker-b01.dom  0xB0047150  30000000  40
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

#define MEMSZ (16u*1024u*1024u)

static uint32_t xlate(Nd500Cpu* c, uint32_t va) {
    if (c->machine && c->machine->mmu_enabled) return nd500_mmu_translate(c, va, 0, 0);
    return va;
}

/* Read a big-endian 32-bit word at a virtual address. */
static int rd32(Nd500Cpu* c, Nd500Machine* m, uint32_t va, uint32_t* out) {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) {
        uint32_t pa = xlate(c, va + i);
        if (pa == 0xFFFFFFFFu || pa >= MEMSZ) return 0;
        v = (v << 8) | nd500_bus_read8(m, pa);
    }
    *out = v;
    return 1;
}

int main(int argc, char** argv) {
    const char* dom = (argc > 1) ? argv[1] : "/mnt/d/ND/500/nd-linker/linker-b01.dom";
    uint32_t stop_pc = (argc > 2) ? (uint32_t)strtoul(argv[2], 0, 0) : 0xB0047150u;
    long maxsteps    = (argc > 3) ? atol(argv[3]) : 30000000L;
    int  maxhits     = (argc > 4) ? atoi(argv[4]) : 40;

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

    m.run_flag = 1; m.stop_reason = STOP_NONE;
    int hits = 0;
    /* ND500X_LOOPARGS_REGS: print the integer registers at the stop PC instead of
     * the frame slots - needed when the value under test lives in a register
     * (e.g. the terminal-type code compared at B0048976). */
    int regs_mode = getenv("ND500X_LOOPARGS_REGS") != NULL;
    if (regs_mode)
        printf("hit#  instr#      PC        I1          I2          I3(W3)      I4\n");
    else
        printf("hit#  instr#      B         b.24(start)  b.32(count)  b.44        b.152(limit)\n");
    for (long i = 0; i < maxsteps && m.run_flag && hits < maxhits; i++) {
        if (c.PC == stop_pc && regs_mode) {
            printf("%-4d  %-10llu  %08X  %-10u  %-10u  %-10u  %u\n",
                   ++hits, (unsigned long long)c.instruction_count, c.PC,
                   c.I[0], c.I[1], c.I[2], c.I[3]);
        } else if (c.PC == stop_pc) {
            uint32_t b24 = 0, b32 = 0, b44 = 0, b152 = 0;
            int ok = rd32(&c, &m, c.B + 24,  &b24)
                   & rd32(&c, &m, c.B + 32,  &b32)
                   & rd32(&c, &m, c.B + 44,  &b44)
                   & rd32(&c, &m, c.B + 152, &b152);
            printf("%-4d  %-10llu  %08X  %-11u  %-11u  %-10u  %u%s\n",
                   ++hits, (unsigned long long)c.instruction_count, c.B,
                   b24, b32, b44, b152, ok ? "" : "  [read failed]");
        }
        nd500_cpu_step(&c);
    }
    printf("\nstop=%s PC=%08X hits=%d\n", nd500_stop_reason_str(m.stop_reason), c.PC, hits);
    nd500_machine_free(&m);
    return 0;
}
