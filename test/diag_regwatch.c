/*
 * Diagnostic: report every change to TOS / B / SP, with the PC that caused it.
 *
 * Motivating question: we initialise TOS=0xB0011ABC, but the linker's stack
 * overflow trap reports TOS=0xB005940C. Something moved it. Grepping the writers
 * of cpu->TOS names several candidates (INIT, ENTM, ENTT, the domain switch,
 * TosSet, Lregbl); only a live watch says which one actually fired, and in what
 * order. Guessing from the candidate list is how hours get lost.
 *
 * Build (make does NOT build this - hand-link against the static libs):
 *   gcc -O2 -o build/bin/diag_regwatch test/diag_regwatch.c \
 *     -Wl,--start-group build/lib/libnd500_debugger.a build/lib/libnd500_cpu.a \
 *     build/lib/libnd500_machine.a build/lib/libmon.a build/lib/libnd500_ndlib.a \
 *     build/lib/libnd500_disasm.a -Wl,--end-group -lpthread -lm
 *
 * Run:
 *   ND500X_REGWATCH_CMD=HELP ./build/bin/diag_regwatch <DOM> <maxsteps> [maxreports]
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

int main(int argc, char** argv) {
    const char* dom = (argc > 1) ? argv[1] : "/mnt/d/ND/500/nd-linker/linker-b01.dom";
    long maxsteps = (argc > 2) ? atol(argv[2]) : 200000L;
    int maxreports = (argc > 3) ? atoi(argv[3]) : 60;

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

    const char* cmd = getenv("ND500X_REGWATCH_CMD");
    if (cmd && *cmd) {
        char line[160];
        snprintf(line, sizeof(line), "%s\r", cmd);
        mon_set_command_buffer(line);
        mon_queue_console_input(line);
    }

    m.run_flag = 1; m.stop_reason = STOP_NONE;
    uint32_t prev_tos = c.TOS;
    int reports = 0;
    fprintf(stderr, "start TOS=%08X B=%08X\n", c.TOS, c.B);
    fprintf(stderr, "instr#      PC(before)  TOS: old -> new        B\n");

    for (long i = 0; i < maxsteps && m.run_flag && reports < maxreports; i++) {
        uint32_t pc_before = c.PC;
        nd500_cpu_step(&c);
        if (c.TOS != prev_tos) {
            fprintf(stderr, "%-10llu  %08X    %08X -> %08X   %08X\n",
                    (unsigned long long)c.instruction_count, pc_before,
                    prev_tos, c.TOS, c.B);
            prev_tos = c.TOS;
            reports++;
        }
    }
    fprintf(stderr, "\nfinal TOS=%08X B=%08X stop=%s PC=%08X\n",
            c.TOS, c.B, nd500_stop_reason_str(m.stop_reason), c.PC);
    nd500_machine_free(&m);
    return 0;
}
