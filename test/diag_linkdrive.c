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
#include <ndmon/mon.h>
#include <ndmon/mon_log.h>
#include <ndmon/mon_file_table.h>
#include "../src/disasm/nd500_disasm.h"

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
    /* ND500X_MMULOG=<0..3>: MMU logging. Level 1 (ERRORS) prints the reason a
     * translation was refused, which is the only way to tell the four distinct
     * protect-violation causes apart. */
    if (getenv("ND500X_MMULOG")) nd500_dbg_set_mmu_log_level(atoi(getenv("ND500X_MMULOG")));
    uint32_t sa=0; int dm=0;
    if (ndlib_dom_load_to_machine(&m,&c,-1,NULL,NULL,&sa,&dm)) { fprintf(stderr,"load2 fail\n"); return 2; }

    fprintf(stderr,"=== DRIVE %s with %d line(s) ===\n", dom, nlines);

    /* Install the console BEFORE the first step.
     *
     * The console is installed as a side effect of mon_queue_console_input(), and
     * this driver used to call that only inside the feed loop - which runs on
     * STOP_WAIT_INPUT. But MON 503B only suspends when a console EXISTS and is
     * empty; with NO console it returns 0 bytes + SUCCESS (deliberate: that is
     * how headless batch programs proceed). So the driver deadlocked on itself:
     *
     *   no console -> terminal reads succeed with 0 bytes -> never suspends
     *              -> never feeds -> never installs a console
     *
     * The ND linker span 123,308 zero-byte reads that way. Queuing an empty
     * string installs the console with nothing in it, so the first terminal read
     * finds it empty and suspends, and the feed loop can do its job. */
    mon_queue_console_input("");

    int fed = 0;
    size_t con_off = 0;
    long total = 0;
    int memtrace_spin = (getenv("ND500X_MEMTRACE_SPIN") != NULL);
    for (int round=0; round<nlines+4; round++) {
        m.run_flag=1; m.stop_reason=STOP_NONE;
        /* Empirical poll-address finder: once at least one command line has been
         * fed (so we are past startup and into the command/config loop that
         * spins), turn on data-read memtrace for this bounded window so the
         * repeatedly-polled datafield address can be histogrammed from stdout. */
        if (memtrace_spin && fed >= 1) { nd500_dbg_set_memtrace(MEMTRACE_READ); }
        long s=0;
        /* ND500X_BREAK_PC=<pc>: report registers each time this PC is reached.
         * The protection violation only happens on the FED path, which the
         * pre-primed harnesses do not reproduce (they diverge at startup), so the
         * observation has to happen inside this driver. */
        uint32_t break_pc = 0;
        if (getenv("ND500X_BREAK_PC")) break_pc = (uint32_t)strtoul(getenv("ND500X_BREAK_PC"), 0, 0);
        static unsigned long long break_after = 0;
        if (getenv("ND500X_BREAK_AFTER")) break_after = strtoull(getenv("ND500X_BREAK_AFTER"),0,0);
        /* ND500X_CALLTRACE_LO/HI: within [instr LO,HI], print PC whenever it lands
         * OUTSIDE the previous instruction's fall-through by more than a page - a
         * cheap call/jump tracer to map a handler's control flow on the fed path. */
        static uint64_t ct_lo=0, ct_hi=0; static uint32_t prev_pc=0;
        if (getenv("ND500X_CALLTRACE_LO")) ct_lo=strtoull(getenv("ND500X_CALLTRACE_LO"),0,0);
        if (getenv("ND500X_CALLTRACE_HI")) ct_hi=strtoull(getenv("ND500X_CALLTRACE_HI"),0,0);
        /* ND500X_WATCH_VA=<va>: watch a 4-byte DATA word; print the writer PC and
         * old->new value each time it changes. Used to find who does (or fails to
         * do) the store to the linker's domain-context flag. */
        static uint32_t watch_va=0; static uint32_t watch_val=0; static int watch_init=0;
        if (getenv("ND500X_WATCH_VA")) watch_va=(uint32_t)strtoul(getenv("ND500X_WATCH_VA"),0,0);
        /* ND500X_KWATCH=1: print PC + I1 whenever the K flag (ST1 bit 8) goes
         * 0->1 within [KWATCH_LO,KWATCH_HI] instr window - finds the exact error
         * setk raise site and the error code (usually in w1/I1). */
        static int kwatch=0; static uint64_t kw_lo=0, kw_hi=~0ull;
        if (getenv("ND500X_KWATCH")) kwatch=1;
        if (getenv("ND500X_KWATCH_LO")) kw_lo=strtoull(getenv("ND500X_KWATCH_LO"),0,0);
        if (getenv("ND500X_KWATCH_HI")) kw_hi=strtoull(getenv("ND500X_KWATCH_HI"),0,0);
        /* ND500X_FTRACE_LO/HI: within [instr LO,HI], print PC + I1..I4 EVERY step, and
         * flag when any I-reg first holds a target value (ND500X_FTRACE_VAL). */
        static uint64_t ft_lo=0, ft_hi=0; static uint32_t ft_val=0xFFFFFFFF;
        if (getenv("ND500X_FTRACE_LO")) ft_lo=strtoull(getenv("ND500X_FTRACE_LO"),0,0);
        if (getenv("ND500X_FTRACE_HI")) ft_hi=strtoull(getenv("ND500X_FTRACE_HI"),0,0);
        if (getenv("ND500X_FTRACE_VAL")) ft_val=(uint32_t)strtoul(getenv("ND500X_FTRACE_VAL"),0,0);
        /* ND500X_DISASM_AT=<pc>: when this PC is first reached (after all feeds),
         * disassemble ND500X_DISASM_LEN bytes from ND500X_DISASM_BASE (default the
         * hit PC) once, to inspect a code region on the fed path. */
        static uint32_t disasm_at=0, disasm_base=0, disasm_len=64; static int disasm_done=0;
        if (getenv("ND500X_DISASM_AT")) disasm_at=(uint32_t)strtoul(getenv("ND500X_DISASM_AT"),0,0);
        if (getenv("ND500X_DISASM_BASE")) disasm_base=(uint32_t)strtoul(getenv("ND500X_DISASM_BASE"),0,0);
        if (getenv("ND500X_DISASM_LEN")) disasm_len=(uint32_t)strtoul(getenv("ND500X_DISASM_LEN"),0,0);
        for (; s<maxsteps && m.run_flag; s++) {
            uint32_t watch_pc_before = c.PC;
            uint32_t k_before = c.ST1 & 0x100u;
            if (disasm_at && !disasm_done && c.PC==disasm_at) {
                disasm_done=1;
                uint32_t base = disasm_base ? disasm_base : c.PC;
                static char dbuf[8192];
                nd500_disasm_format_range(&m, base, disasm_len, dbuf, sizeof(dbuf));
                fprintf(stderr,"=== DISASM @%08X len=%u (hit PC=%08X instr=%llu) ===\n%s\n=== END DISASM ===\n",
                        base, disasm_len, c.PC, (unsigned long long)c.instruction_count, dbuf);
            }
            if (break_pc && c.PC == break_pc && c.instruction_count >= break_after) {
                fprintf(stderr, "[BREAK] PC=%08X instr=%llu B=%08X R=%08X I1=%08X I2=%08X I3=%08X I4=%08X ST1=%08X\n",
                        c.PC, (unsigned long long)c.instruction_count, c.B, c.R, c.I[0], c.I[1], c.I[2], c.I[3], c.ST1);
                const char* dmp = getenv("ND500X_BREAK_DUMP");
                const char* dmpb = getenv("ND500X_BREAK_DUMP_BREL");
                if (dmpb) {
                    /* dump at B + signed offset (frame-relative) - survives frame aliasing */
                    int32_t off = (int32_t)strtol(dmpb, 0, 0);
                    uint32_t va = c.B + (uint32_t)off;
                    int dlen = getenv("ND500X_BREAK_DUMPLEN") ? atoi(getenv("ND500X_BREAK_DUMPLEN")) : 16;
                    for (int _row=0; _row<dlen; _row+=16) {
                        fprintf(stderr, "[BDUMP B%+d @%08X]:", off+_row, va+_row);
                        for (int _b=0;_b<16 && _row+_b<dlen;_b++){
                            uint32_t a = va+_row+_b;
                            uint32_t pa = c.machine->mmu_enabled ? nd500_mmu_translate(&c, a,0,0) : a;
                            fprintf(stderr, " %02X", (pa!=0xFFFFFFFFu && pa<MEMSZ)? nd500_bus_read8(&m,pa):0);
                        }
                        fprintf(stderr, "\n");
                    }
                }
                if (dmp) {
                    uint32_t va = (uint32_t)strtoul(dmp, 0, 0);
                    int dlen = getenv("ND500X_BREAK_DUMPLEN") ? atoi(getenv("ND500X_BREAK_DUMPLEN")) : 16;
                    for (int _row=0; _row<dlen; _row+=16) {
                        fprintf(stderr, "[DUMP] %08X:", va+_row);
                        char ascii[17]; int an=0;
                        for (int _b=0;_b<16 && _row+_b<dlen;_b++){
                            uint32_t a = va+_row+_b;
                            uint32_t pa = c.machine->mmu_enabled ? nd500_mmu_translate(&c, a,0,0) : a;
                            uint8_t v = (pa!=0xFFFFFFFFu && pa<MEMSZ)? nd500_bus_read8(&m,pa):0;
                            fprintf(stderr, " %02X", v);
                            ascii[an++] = (v>=32 && v<127)? (char)v : '.';
                        }
                        ascii[an]=0;
                        fprintf(stderr, "  |%s|\n", ascii);
                    }
                }
            }
            if (ct_hi && c.instruction_count>=ct_lo && c.instruction_count<=ct_hi) {
                int32_t d = (int32_t)(c.PC - prev_pc);
                if (d> 8 || d< -4) fprintf(stderr,"[CT %llu] %08X -> %08X\n",
                    (unsigned long long)c.instruction_count, prev_pc, c.PC);
                prev_pc = c.PC;
            }
            nd500_cpu_step(&c);
            if (ft_hi && c.instruction_count>=ft_lo && c.instruction_count<=ft_hi) {
                int hit = (c.I[0]==ft_val||c.I[1]==ft_val||c.I[2]==ft_val||c.I[3]==ft_val);
                fprintf(stderr,"[FT %llu] PC=%08X I=%08X %08X %08X %08X%s\n",
                        (unsigned long long)c.instruction_count, watch_pc_before,
                        c.I[0],c.I[1],c.I[2],c.I[3], hit?"  <== VAL":"");
            }
            if (kwatch && !k_before && (c.ST1 & 0x100u)
                && c.instruction_count>=kw_lo && c.instruction_count<=kw_hi) {
                fprintf(stderr,"[KSET] PC=%08X (raised at %08X) I1=%08X instr=%llu B=%08X\n",
                        c.PC, watch_pc_before, c.I[0],
                        (unsigned long long)c.instruction_count, c.B);
            }
            if (watch_va) {
                uint32_t pa = c.machine->mmu_enabled ? nd500_mmu_translate(&c, watch_va, 0, 0) : watch_va;
                if (pa != 0xFFFFFFFFu && pa+3 < MEMSZ) {
                    uint32_t v = ((uint32_t)nd500_bus_read8(&m,pa)<<24)|((uint32_t)nd500_bus_read8(&m,pa+1)<<16)
                               | ((uint32_t)nd500_bus_read8(&m,pa+2)<<8)|(uint32_t)nd500_bus_read8(&m,pa+3);
                    if (!watch_init) { watch_val=v; watch_init=1; }
                    else if (v != watch_val) {
                        fprintf(stderr,"[WATCH %08X] %08X -> %08X  by PC=%08X instr=%llu B=%08X\n",
                                watch_va, watch_val, v, watch_pc_before,
                                (unsigned long long)c.instruction_count, c.B);
                        watch_val=v;
                    }
                }
            }
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
                /* stop_data holds the waiting device: 0 = command buffer,
                 * >0 = terminal/console (503B/INBT device 1). Route the fed line
                 * to the channel the linker is actually reading from. */
                uint32_t wdev = m.stop_data;
                fprintf(stderr,"  --> feeding line[%d]=\"%s\" (waited on device %u) to BOTH channels\n",
                        fed, lines[fed], wdev);
                /* The linker reads the command via BOTH the resident device-0
                 * command-buffer poll AND 503B DVINST on the terminal; feed both
                 * so whichever reader is active is satisfied. ND500X_LINKDRIVE_ONE
                 * restricts to just the waiting device for isolation experiments. */
                if (getenv("ND500X_LINKDRIVE_ONE")) {
                    if (wdev == 0) mon_set_command_buffer(lines[fed]);
                    else           mon_queue_console_input(lines[fed]);
                } else {
                    mon_set_command_buffer(lines[fed]);
                    mon_queue_console_input(lines[fed]);
                }
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
