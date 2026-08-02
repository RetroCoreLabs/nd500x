/*
 * Diagnostic: dump the linker's exact PC sequence for one cycle.
 *
 * The linker repeats a closed, byte-identical 937-instruction cycle forever
 * (proven by test/diag_loopargs.c and test/diag_pollwatch.c). This dumps the PC
 * sequence in execution order so the cycle's structure - in particular the
 * backward branch that closes it, and the conditional that should have exited -
 * can be read directly instead of inferred.
 *
 * Build (make does NOT build this - hand-link against the static libs):
 *   gcc -O2 -o build/bin/diag_pcseq test/diag_pcseq.c \
 *     -Wl,--start-group build/lib/libnd500_debugger.a build/lib/libnd500_cpu.a \
 *     build/lib/libnd500_machine.a build/lib/libmon.a build/lib/libnd500_ndlib.a \
 *     build/lib/libnd500_disasm.a -Wl,--end-group -lpthread -lm
 *
 * Run:
 *   ./build/bin/diag_pcseq [DOM] [skip] [window]
 *   default: linker-b01.dom  36020  937
 */
#include <stdio.h>
#include <stdlib.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/cpu/nd500_domain.h"
#include "../src/ndlib/ndlib.h"
#include <ndmon/mon.h>
#include "testdata.h"

#define MEMSZ (16u*1024u*1024u)

int main(int argc, char** argv) {
    const char* dom = (argc > 1) ? argv[1] : nd500_testdata("nd-linker/linker-b01.dom");
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
    for (long i = 0; i < skip && m.run_flag; i++) nd500_cpu_step(&c);

    /* Emit "seq PC B ST1" per instruction, in execution order, to stderr so it
     * is not tangled with the guest console on stdout. */
    for (long i = 0; i < window && m.run_flag; i++) {
        fprintf(stderr, "%ld %08X B=%08X ST1=%08X\n",
                i, c.PC, c.B, c.ST1);
        nd500_cpu_step(&c);
    }
    nd500_machine_free(&m);
    return 0;
}
