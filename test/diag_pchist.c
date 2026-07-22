/*
 * Diagnostic: find WHERE the linker spins after its startup dialogue.
 *
 * Evidence that motivates this: over 5M instructions the linker issues ~29 MON
 * calls, all clustered at startup, then no I/O at all - so it is not blocked on
 * input, it is looping internally. The PC is not pinned to one address, so the
 * loop body spans a range. This harness histograms the PC over a window of
 * execution (skipping the startup phase) and prints the hottest addresses, which
 * names the loop body for disassembly.
 *
 * Build (make does NOT build this - hand-link against the static libs):
 *   gcc -O2 -o build/bin/diag_pchist test/diag_pchist.c \
 *     -Wl,--start-group build/lib/libnd500_debugger.a build/lib/libnd500_cpu.a \
 *     build/lib/libnd500_machine.a build/lib/libmon.a build/lib/libnd500_ndlib.a \
 *     build/lib/libnd500_disasm.a -Wl,--end-group -lpthread -lm
 *
 * Run:
 *   ./build/bin/diag_pchist [DOM] [skip] [window]
 *   default: linker-b01.dom  1000000  2000000
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

#define MEMSZ (16u*1024u*1024u)
#define HSIZE 65536u          /* open-addressed PC histogram */

static uint32_t h_pc[HSIZE];
static uint64_t h_ct[HSIZE];

static void h_add(uint32_t pc) {
    uint32_t i = (pc * 2654435761u) % HSIZE;
    for (uint32_t n = 0; n < HSIZE; n++) {
        uint32_t k = (i + n) % HSIZE;
        if (h_ct[k] == 0) { h_pc[k] = pc; h_ct[k] = 1; return; }
        if (h_pc[k] == pc) { h_ct[k]++; return; }
    }
}

int main(int argc, char** argv) {
    const char* dom = (argc > 1) ? argv[1] : "/mnt/d/ND/500/nd-linker/linker-b01.dom";
    long skip   = (argc > 2) ? atol(argv[2]) : 1000000L;
    long window = (argc > 3) ? atol(argv[3]) : 2000000L;

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

    /* The linker uses a DUAL input source: the command line arrives once via
     * 503B DVINST on the terminal, and the characters are read via 1B INBT on
     * device 0 (the command buffer). Priming only one of them sends it down an
     * unreachable path - which is exactly how the earlier "closed deterministic
     * loop" misdiagnosis happened. ND500X_PCHIST_CMD primes BOTH, matching
     * test/diag_linkdrive.c. */
    const char* cmd = getenv("ND500X_PCHIST_CMD");
    if (cmd && *cmd) {
        char line[160];
        snprintf(line, sizeof(line), "%s\r", cmd);
        mon_set_command_buffer(line);
        mon_queue_console_input(line);
        fprintf(stderr, "[primed both channels] %s\n", cmd);
    }

    m.run_flag = 1; m.stop_reason = STOP_NONE;
    for (long i = 0; i < skip && m.run_flag; i++) nd500_cpu_step(&c);

    uint32_t lo = 0xFFFFFFFFu, hi = 0;
    long n = 0;
    for (; n < window && m.run_flag; n++) {
        uint32_t pc = c.PC;
        h_add(pc);
        if (pc < lo) lo = pc;
        if (pc > hi) hi = pc;
        nd500_cpu_step(&c);
    }

    /* Top 25 hottest PCs. */
    printf("sampled %ld instr after skipping %ld; PC range %08X..%08X\n", n, skip, lo, hi);
    printf("stop=%s PC=%08X\n\n", nd500_stop_reason_str(m.stop_reason), c.PC);
    printf("  count      PC        %% of window\n");
    for (int top = 0; top < 25; top++) {
        uint32_t best = 0; uint64_t bc = 0;
        for (uint32_t k = 0; k < HSIZE; k++)
            if (h_ct[k] > bc) { bc = h_ct[k]; best = k; }
        if (bc == 0) break;
        printf("  %-9llu  %08X  %5.2f%%\n",
               (unsigned long long)bc, h_pc[best], 100.0 * (double)bc / (double)n);
        h_ct[best] = 0;
    }
    nd500_machine_free(&m);
    return 0;
}
