/* diag_linkfeed.c - Drive the ND linker by feeding its SINTRAN COMMAND BUFFER.
 *
 * The linker reads its commands from logical device 0 (the SINTRAN command
 * buffer), not the terminal: 143B RSIO reports input=0, and it then polls
 * 1B INBT on device 0. On an empty buffer it either busy-spins (if we return
 * EOF) or suspends (current behaviour), so it must actually be FED.
 *
 * This feeds successive command lines into the command buffer each time the
 * machine stops with STOP_WAIT_INPUT, and reports where it gets to.
 *
 *   argv[1] = DOM   argv[2] = ';'-separated commands   argv[3] = maxsteps
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
#include <ndmon/mon_log.h>
#include <ndmon/mon_file_table.h>
#include <ndmon/mon_clock.h>
#define MEMSZ (16u*1024u*1024u)

int main(int argc, char** argv) {
    const char* dom = (argc > 1) ? argv[1] : "/mnt/d/ND/500/nd-linker/linker-b01.dom";
    const char* cmds = (argc > 2) ? argv[2] : "LIST-STATUS";
    long maxsteps = (argc > 3) ? atol(argv[3]) : 2000000;

    if (ndlib_load_dom_header(dom) || ndlib_load_dom_segments()) { fprintf(stderr, "load fail\n"); return 2; }
    Nd500Machine m; Nd500Cpu c;
    nd500_machine_init(&m, MEMSZ); nd500_cpu_init(&c, &m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    mon_log_enable(0);
    if (getenv("ND500X_MONLOG")) { mon_log_enable(1); mon_log_set_level(MON_LOG_DEBUG); }

    /* Split the ';'-separated command list. */
    char buf[512]; snprintf(buf, sizeof(buf), "%s", cmds);
    char* lines[32]; int nlines = 0;
    for (char* t = strtok(buf, ";"); t && nlines < 32; t = strtok(NULL, ";")) lines[nlines++] = t;
    int next = 0;

    uint32_t sa = 0; int dm = 0;
    if (ndlib_dom_load_to_machine(&m, &c, -1, NULL, NULL, &sa, &dm)) { fprintf(stderr, "load2 fail\n"); return 2; }
    printf("Loaded '%s' entry=0x%08X\n", dom, c.PC);

    /* Prime the command buffer with the first line before starting. */
    if (next < nlines) {
        char line[160]; snprintf(line, sizeof(line), "%s\r", lines[next]);
        printf("[feed cmdbuf] %s\n", lines[next]);
        mon_set_command_buffer(line);
        next++;
    }

    m.run_flag = 1; m.stop_reason = STOP_NONE;
    long steps = 0;
    int feeds = 0;
    while (steps < maxsteps) {
        while (steps < maxsteps && m.run_flag) { nd500_cpu_step(&c); steps++; }
        if (m.stop_reason == STOP_WAIT_INPUT && feeds < 64) {
            /* Feed the next command line and resume. */
            if (next < nlines) {
                char line[160]; snprintf(line, sizeof(line), "%s\r", lines[next]);
                printf("[feed cmdbuf @instr=%llu dev=%u] %s\n",
                       (unsigned long long)c.instruction_count, m.stop_data, lines[next]);
                mon_set_command_buffer(line);
                next++;
            } else {
                printf("[no more commands; stopping at instr=%llu]\n",
                       (unsigned long long)c.instruction_count);
                break;
            }
            feeds++;
            m.stop_reason = STOP_NONE; m.run_flag = 1;
            continue;
        }
        break;
    }
    printf("STOP=%s instr=%llu PC=%08X\n", nd500_stop_reason_str(m.stop_reason),
           (unsigned long long)c.instruction_count, c.PC);
    const char* out = mon_get_console_output();
    if (out && *out) printf("---- linker console output ----\n%s\n------------------------------\n", out);
    return 0;
}
