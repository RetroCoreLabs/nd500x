/* diag_linkterm.c - Drive the ND linker through its TERMINAL (console).
 *
 * Companion to diag_linkfeed.c, which drives the linker through the SINTRAN
 * COMMAND BUFFER (logical device 0). Device 0 is the BATCH channel: feeding it
 * forces the linker into its batch dialogue ("Batch abortion (Yes,No)Yes")
 * where it spins without consuming the fed commands. Since 143B RSIO reports
 * the TERMINAL as command input for an interactive program (2026-07-16), the
 * linker takes its interactive path and reads via 503B DVINST / 511B DVIO -
 * i.e. the CONSOLE. This harness feeds that.
 *
 * It also PRINTS THE CONSOLE OUTPUT at every stop: mon_queue_console_input()
 * installs a ConsoleIO that captures guest output into an internal buffer, so
 * prompts are INVISIBLE on stdout unless the harness asks for them.
 *
 *   argv[1] = DOM   argv[2] = ';'-separated command lines   argv[3] = maxsteps
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
#include "../src/libmon/mon_log.h"
#include "../src/libmon/mon_file_table.h"
#include "../src/libmon/mon_clock.h"
#define MEMSZ (16u*1024u*1024u)

/* Print whatever the guest has written since the last call.
 *
 * MUST print by LENGTH, not with printf("%s"). The guest's output contains
 * embedded NUL bytes - the linker's banner starts
 *   0D 0A 0D 0A 00 00 00 00 00 00 00 29 4E 44 4C ...  ("\r\n\r\n" then NULs then ")NDL")
 * so a "%s" print stops at byte 4 and the whole banner looks like it was never
 * written. That cost real time twice: the console capture was always correct,
 * the PRINTER was truncating it.
 *
 * NULs and other non-printables are shown as '.', with the raw hex available via
 * ND500X_CONSOLE_HEX for anything that is not really text. */
static size_t g_shown = 0;
static void show_console(const char* tag) {
    const char* out = mon_get_console_output();
    if (!out) return;
    size_t len = mon_get_console_output_len();
    if (len <= g_shown) return;

    printf("---- console (%s) %zu new byte(s) ----\n", tag, len - g_shown);
    for (size_t i = g_shown; i < len; i++) {
        unsigned char ch = (unsigned char)out[i];
        if (ch == '\r') continue;                 /* CR: keep the transcript readable */
        if (ch == '\n' || (ch >= 0x20 && ch < 0x7F)) putchar(ch);
        else putchar('.');                        /* NUL / BEL / binary */
    }
    putchar('\n');
    if (getenv("ND500X_CONSOLE_HEX")) {
        printf("---- hex ----\n");
        for (size_t i = g_shown; i < len; i++) {
            printf("%02X ", (unsigned char)out[i]);
            if (((i - g_shown) & 31) == 31) putchar('\n');
        }
        putchar('\n');
    }
    printf("----------------------\n");
    g_shown = len;
}

int main(int argc, char** argv) {
    const char* dom = (argc > 1) ? argv[1] : "/mnt/d/ND/500/nd-linker/linker-b01.dom";
    const char* cmds = (argc > 2) ? argv[2] : "LIST-STATUS";
    long maxsteps = (argc > 3) ? atol(argv[3]) : 2000000;

    if (ndlib_load_dom_header(dom) || ndlib_load_dom_segments()) {
        fprintf(stderr, "load fail\n"); return 2;
    }
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
    if (ndlib_dom_load_to_machine(&m, &c, -1, NULL, NULL, &sa, &dm)) {
        fprintf(stderr, "load2 fail\n"); return 2;
    }
    printf("Loaded '%s' entry=0x%08X\n", dom, c.PC);

    /* Prime the terminal with the first line before starting. */
    if (next < nlines) {
        char line[160]; snprintf(line, sizeof(line), "%s\r", lines[next]);
        printf("[feed term] %s\n", lines[next]);
        mon_queue_console_input(line);
        next++;
    }

    m.run_flag = 1; m.stop_reason = STOP_NONE;
    long steps = 0;
    int feeds = 0;
    while (steps < maxsteps) {
        while (steps < maxsteps && m.run_flag) { nd500_cpu_step(&c); steps++; }
        if (m.stop_reason == STOP_WAIT_INPUT && feeds < 64) {
            show_console("prompt");
            if (next < nlines) {
                char line[160]; snprintf(line, sizeof(line), "%s\r", lines[next]);
                /* Route the feed to whichever channel is actually waiting.
                 * The linker uses BOTH: device 1 (terminal) for its startup
                 * dialogue via 511B DVIO, and device 0 (the SINTRAN command
                 * buffer) for its command loop via 1B INBT. m.stop_data carries
                 * the waiting device (ctx->wait_device). */
                if (m.stop_data == 0) {
                    /* Device 0 is the SINTRAN COMMAND BUFFER, which per the manual
                     * (ND-860228.2, MON 1B INBT / MON 12B SETCM) holds "the last
                     * command input from the terminal" - i.e. the invocation line,
                     * max 32 chars - and is how a program reads "parameters
                     * following the program name". It is NOT a command stream.
                     * ND500X_CMDBUF_TERM=1 answers it with a bare terminator,
                     * meaning "no parameters", to test whether the linker then
                     * takes its commands from the terminal instead. */
                    const char* t = getenv("ND500X_CMDBUF_TERM");
                    if (t) {
                        printf("[feed CMDBUF @instr=%llu dev=%u] <terminator '%s'> (no params)\n",
                               (unsigned long long)c.instruction_count, m.stop_data, t);
                        mon_set_command_buffer(t);
                        feeds++;
                        m.stop_reason = STOP_NONE; m.run_flag = 1;
                        continue;   /* do NOT consume a command line for this */
                    }
                    printf("[feed CMDBUF @instr=%llu dev=%u] %s\n",
                           (unsigned long long)c.instruction_count, m.stop_data, lines[next]);
                    mon_set_command_buffer(line);
                } else {
                    printf("[feed TERM   @instr=%llu dev=%u] %s\n",
                           (unsigned long long)c.instruction_count, m.stop_data, lines[next]);
                    mon_queue_console_input(line);
                }
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
    show_console("final");
    printf("STOP=%s instr=%llu PC=%08X steps=%ld\n", nd500_stop_reason_str(m.stop_reason),
           (unsigned long long)c.instruction_count, c.PC, steps);
    return 0;
}
