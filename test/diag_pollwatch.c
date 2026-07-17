/*
 * Diagnostic: find WHAT the linker is polling in its infinite cycle.
 *
 * Established by test/diag_loopargs.c: from instr 36020 the linker repeats one
 * 937-instruction cycle forever - identical frame (B=B00020B4) and identical
 * args (start=1, count=15) on every pass - and issues ZERO MON calls while doing
 * it. With no MON calls it cannot be polling I/O through SINTRAN, so it must be
 * reading MEMORY and waiting for a value that never changes.
 *
 * This harness enables memtrace for exactly one cycle and reports data addresses
 * that are READ but NEVER WRITTEN during it. Such an address is what the linker
 * is waiting on - i.e. a field real SINTRAN maintains that nd500x never updates.
 *
 * Instruction fetches are excluded: they go through fetch_8 (PROGRAM SPACE),
 * which this parser ignores; only read_8/16/32/64 count as data reads.
 *
 * Build (make does NOT build this - hand-link against the static libs):
 *   gcc -O2 -o build/bin/diag_pollwatch test/diag_pollwatch.c \
 *     -Wl,--start-group build/lib/libnd500_debugger.a build/lib/libnd500_cpu.a \
 *     build/lib/libnd500_machine.a build/lib/libmon.a build/lib/libnd500_ndlib.a \
 *     build/lib/libnd500_disasm.a -Wl,--end-group -lpthread -lm
 *
 * Run (pipe the raw trace to a file, then it prints the summary):
 *   ./build/bin/diag_pollwatch [DOM] [skip] [window] > /tmp/trace.txt
 *   default: linker-b01.dom  36020  937
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/cpu/nd500_domain.h"
#include "../src/ndlib/ndlib.h"
#include "../src/libmon/mon.h"

#define MEMSZ (16u*1024u*1024u)

int main(int argc, char** argv) {
    const char* dom = (argc > 1) ? argv[1] : "/mnt/d/ND/500/nd-linker/linker-b01.dom";
    long skip   = (argc > 2) ? atol(argv[2]) : 36020L;
    long window = (argc > 3) ? atol(argv[3]) : 937L;

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
    nd500_dbg_set_memtrace(MEMTRACE_OFF);
    for (long i = 0; i < skip && m.run_flag; i++) nd500_cpu_step(&c);

    /* Trace exactly one cycle. The raw [MEMTRACE] lines go to stdout; pipe them
     * to a file and post-process (read-set minus write-set). */
    fprintf(stderr, "=== tracing %ld instr from %ld (PC=%08X B=%08X)\n",
            window, skip, c.PC, c.B);
    nd500_dbg_set_memtrace(MEMTRACE_ALL);
    for (long i = 0; i < window && m.run_flag; i++) nd500_cpu_step(&c);
    nd500_dbg_set_memtrace(MEMTRACE_OFF);
    fprintf(stderr, "=== done, PC=%08X\n", c.PC);

    nd500_machine_free(&m);
    return 0;
}
