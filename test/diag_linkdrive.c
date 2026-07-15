/*
 * Diagnostic: DRIVE the ND LINKER (or any DOM) line-by-line across STOP_WAIT_INPUT.
 *
 * Unlike diag_linkmon (which pre-loads all input up front and lets it run),
 * this feeds ONE command line into the SINTRAN command buffer (logical device 0)
 * each time the CPU suspends on a blocking read (STOP_WAIT_INPUT), mimicking a
 * real terminal refilling the command buffer on demand. Between feeds it prints
 * the console output emitted since the previous feed, so the exact prompt that
 * triggered each read is visible - this is how we discover the linker's
 * prompt/answer sequence (Phase 3, requirement A).
 *
 * argv: <dom-path> <script>   script lines separated by ";;" (each -> CR)
 * Run PINNED from a sandbox:  ND500X_PIN_CLOCK=1 ../bin/diag_linkdrive <dom> "<script>"
 * Env: ND500X_NOLOG=1 to silence MON logging; ND500X_MAXSTEPS to cap per-segment steps.
 * Build: gcc -O2 -o build/bin/diag_linkdrive test/diag_linkdrive.c \
 *   -Wl,--start-group build/lib/libnd500_debugger.a build/lib/libnd500_cpu.a \
 *   build/lib/libnd500_machine.a build/lib/libmon.a build/lib/libnd500_ndlib.a \
 *   build/lib/libnd500_disasm.a -Wl,--end-group -lpthread -lm
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

#define MEMSZ (16u*1024u*1024u)
#define MAX_LINES 64

/* Print the console output that has been produced since offset `from`, with
 * control chars shown as ^X / \xNN so prompts are readable. Returns new length. */
static size_t dump_console_delta(size_t from, const char* tag) {
    const char* co = mon_get_console_output();
    size_t cl = mon_get_console_output_len();
    if (!co || cl <= from) { return cl; }
    fprintf(stderr, "  [%s console+%zu..%zu]: \"", tag, from, cl);
    for (size_t i = from; i < cl; i++) {
        unsigned char c = (unsigned char)co[i];
        if (c == '\r') fprintf(stderr, "\\r");
        else if (c == '\n') fprintf(stderr, "\\n");
        else if (c == 0x07) fprintf(stderr, "^G");
        else if (c >= 32 && c < 127) fputc(c, stderr);
        else fprintf(stderr, "\\x%02X", c);
    }
    fprintf(stderr, "\"\n");
    return cl;
}

int main(int argc, char** argv) {
    const char* dom = (argc>1)?argv[1]:"/mnt/d/ND/500/nd-linker/linker-b01.dom";
    const char* script = (argc>2)?argv[2]:"EXIT;;";
    long maxsteps = getenv("ND500X_MAXSTEPS") ? strtol(getenv("ND500X_MAXSTEPS"),NULL,0) : 8000000L;

    /* Split script into lines on ";;" - each becomes one CR-terminated command. */
    char lines[MAX_LINES][128];
    int nlines = 0;
    {
        const char* p = script;
        char cur[128]; size_t k = 0;
        while (*p && nlines < MAX_LINES) {
            if (p[0]==';' && p[1]==';') {
                cur[k]=0;
                snprintf(lines[nlines], sizeof(lines[0]), "%s\r", cur);
                nlines++; k=0; p+=2;
            } else if (k < sizeof(cur)-1) { cur[k++]=*p++; }
            else p++;
        }
        if (k>0 && nlines<MAX_LINES) { cur[k]=0; snprintf(lines[nlines],sizeof(lines[0]),"%s\r",cur); nlines++; }
    }

    if (ndlib_load_dom_header(dom) || ndlib_load_dom_segments()) { fprintf(stderr,"load fail\n"); return 2; }
    Nd500Machine m; Nd500Cpu c;
    nd500_machine_init(&m,MEMSZ); nd500_cpu_init(&c,&m); nd500_cpu_reset(&c);
    nd500_mmu_init(&c); nd500_domain_init(&c); mon_init();
    if (!getenv("ND500X_NOLOG")) { mon_log_enable(1); mon_log_set_level(MON_LOG_DEBUG); }
    uint32_t sa=0; int dm=0;
    if (ndlib_dom_load_to_machine(&m,&c,-1,NULL,NULL,&sa,&dm)) { fprintf(stderr,"load2 fail\n"); return 2; }

    fprintf(stderr,"=== DRIVE %s with %d line(s) ===\n", dom, nlines);
    int fed = 0;
    size_t con_off = 0;
    long total = 0;
    for (int round=0; round<nlines+4; round++) {
        m.run_flag=1; m.stop_reason=STOP_NONE;
        long s=0;
        for (; s<maxsteps && m.run_flag; s++) {
            nd500_cpu_step(&c);
            if (m.run_flag==0 && m.stop_reason!=STOP_NONE) break;
        }
        total += s;
        con_off = dump_console_delta(con_off, "out");
        const char* sr = nd500_stop_reason_str(m.stop_reason);
        fprintf(stderr,"  round=%d stop=%s instr=%llu PC=%08X (fed %d/%d)\n",
                round, sr, (unsigned long long)c.instruction_count, c.PC, fed, nlines);

        if (m.stop_reason == STOP_MON_HALT) { fprintf(stderr,"=== CLEAN EXIT (MON 0B LEAVE) ===\n"); break; }
        if (m.stop_reason == STOP_WAIT_INPUT) {
            if (fed < nlines) {
                fprintf(stderr,"  --> feeding line[%d]=\"%s\"\n", fed, lines[fed]);
                mon_set_command_buffer(lines[fed]);
                fed++;
                continue;
            } else {
                fprintf(stderr,"=== SUSPENDED, script exhausted (need more input) ===\n");
                break;
            }
        }
        /* Any other stop (trap, unimplemented, max steps) -> report and end. */
        fprintf(stderr,"=== STOPPED: %s ===\n", sr);
        break;
    }
    fprintf(stderr,"total instr executed this drive: %ld\n", total);
    return 0;
}
