/*
 * Diagnostic: count executions of an arbitrary list of PCs.
 *
 * diag_pchist only reports the top-25 hottest PCs, which hides low-frequency
 * but decisive sites (an error raiser that fires 20 times still names the bug).
 * This harness takes an explicit PC list and reports the exact hit count for
 * each, so a hypothesis about "which of these N sites actually executes" can be
 * answered in ONE run instead of N.
 *
 * Both input channels are primed (command buffer + terminal), matching
 * test/diag_linkdrive.c - priming only one sends the linker down an unreachable
 * path and produces a convincing false result.
 *
 * Build (make does NOT build this - hand-link against the static libs):
 *   gcc -O2 -o build/bin/diag_pccount test/diag_pccount.c \
 *     -Wl,--start-group build/lib/libnd500_debugger.a build/lib/libnd500_cpu.a \
 *     build/lib/libnd500_machine.a build/lib/libmon.a build/lib/libnd500_ndlib.a \
 *     build/lib/libnd500_disasm.a -Wl,--end-group -lpthread -lm
 *
 * Run:
 *   ND500X_PCCOUNT_CMD=HELP ./build/bin/diag_pccount <DOM> <maxsteps> <pc> [pc...]
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
#include "../src/libmon/mon_file_table.h"

#define MEMSZ (16u*1024u*1024u)
#define MAXPC 64

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s <DOM> <maxsteps> <pc> [pc...]\n", argv[0]);
        return 2;
    }
    const char* dom = argv[1];
    long maxsteps = atol(argv[2]);

    uint32_t pcs[MAXPC];
    uint64_t hits[MAXPC], first[MAXPC], last[MAXPC];
    int npc = 0;
    for (int i = 3; i < argc && npc < MAXPC; i++) {
        pcs[npc] = (uint32_t)strtoul(argv[i], 0, 0);
        hits[npc] = 0; first[npc] = 0; last[npc] = 0;
        npc++;
    }

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

    const char* cmd = getenv("ND500X_PCCOUNT_CMD");
    if (cmd && *cmd) {
        char line[160];
        snprintf(line, sizeof(line), "%s\r", cmd);
        mon_set_command_buffer(line);
        mon_queue_console_input(line);
        fprintf(stderr, "[primed both channels] %s\n", cmd);
    }

    m.run_flag = 1; m.stop_reason = STOP_NONE;
    long n = 0;
    for (; n < maxsteps && m.run_flag; n++) {
        uint32_t pc = c.PC;
        for (int k = 0; k < npc; k++)
            if (pc == pcs[k]) {
                if (hits[k] == 0) first[k] = c.instruction_count;
                last[k] = c.instruction_count;
                hits[k]++;
                break;
            }
        nd500_cpu_step(&c);
    }

    printf("\nran %ld instr; stop=%s PC=%08X\n\n", n, nd500_stop_reason_str(m.stop_reason), c.PC);
    printf("  PC        hits        first@instr   last@instr\n");
    for (int k = 0; k < npc; k++)
        printf("  %08X  %-10llu  %-12llu  %llu\n", pcs[k],
               (unsigned long long)hits[k], (unsigned long long)first[k],
               (unsigned long long)last[k]);

    nd500_machine_free(&m);
    return 0;
}
