/*
 * Diagnostic: watch a fixed virtual address for writes, and dump the crash chain.
 *
 * Answers C# Q1/Q3 for the NC crash: the bogus base pointer P = mem[B+0x14] is
 * read at 0x080241F4. This logs every change to that absolute slot (default
 * 0x100001F4 = B+0x14 with B=0x100001E0) with instr#, PC, old->new, so we find
 * the LAST writer of 0x34 before the crash - deciding "uninitialized/stale" vs
 * "written-wrong". At stop it also prints B, mem[B+0x14], mem[P+2].
 *
 * Run FROM build/nc_sandbox:
 *   ND500X_PIN_CLOCK=1 ../bin/diag_watch_addr "CHECK B,B,B\r" 0x100001F4
 *
 * Build (same static-lib link line as diag_crash_ea.c).
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
#include <ndmon/mon_clock.h>
#include "../src/disasm/nd500_disasm.h"

#define MEMORY_SIZE   (16u * 1024u * 1024u)
#define MAX_STEPS     2000000L
static const char* DOM = "/mnt/d/ND/500/FraTor/nc/nc-a06.dom";
static const char* CMD = "CHECK B,B,B\r";

static void prime_input(void) {
    char inbuf[128]; size_t j = 0;
    for (size_t i = 0; CMD[i] && j < sizeof(inbuf) - 2; i++) {
        if (CMD[i] == '\\' && CMD[i + 1] == 'r') { inbuf[j++] = '\r'; i++; }
        else if (CMD[i] == ';' && CMD[i + 1] == ';') { inbuf[j++] = '\r'; i++; }
        else inbuf[j++] = CMD[i];
    }
    inbuf[j] = 0;
    mon_queue_console_input(inbuf);
}

static uint32_t xlate(Nd500Cpu* c, uint32_t v) {
    if (c->machine && c->machine->mmu_enabled) return nd500_mmu_translate(c, v, 0, 0);
    return v;
}

int main(int argc, char** argv) {
    if (argc > 1) CMD = argv[1];
    uint32_t WATCH = (argc > 2) ? (uint32_t)strtoul(argv[2], 0, 0) : 0x100001F4u;

    if (ndlib_load_dom_header(DOM) != 0) { fprintf(stderr, "header load failed\n"); return 2; }
    if (ndlib_load_dom_segments() != 0)  { fprintf(stderr, "segment load failed\n"); return 2; }

    Nd500Machine m; Nd500Cpu c;
    nd500_machine_init(&m, MEMORY_SIZE);
    if (!m.memory) return 2;
    nd500_cpu_init(&c, &m);
    nd500_cpu_reset(&c);
    nd500_mmu_init(&c);
    nd500_domain_init(&c);
    mon_init();
    prime_input();
    uint32_t sa = 0; int dom = 0;
    if (ndlib_dom_load_to_machine(&m, &c, -1, NULL, NULL, &sa, &dom) != 0) return 2;
    m.run_flag = 1; m.stop_reason = STOP_NONE;

    (void)WATCH;
    printf("CMD=%s : snapshot at call 0x08024183 (arg=I0) and P-load 0x080241F4 (B, mem[B+0x14])\n", CMD);

    #define NLOG 40
    struct { unsigned long long ic; uint32_t pc, a, b, cval; } log[NLOG];
    int ln = 0;
    long step = 0;
    for (; step < MAX_STEPS && m.run_flag; step++) {
        uint32_t pc = c.PC;
        if (pc == 0x08024180u) {
            /* w1 := r1.(0x1): here I0 = p (caller local b.0x14). Capture p, mem[p+0], mem[p+1]. */
            unsigned long long ic = c.instruction_count;
            uint32_t p = c.I[0], m0 = 0, m1 = 0;
            if (c.machine && c.machine->mmu_enabled) {
                uint32_t pp0 = nd500_mmu_translate(&c, p, 0, 0);
                uint32_t pp1 = nd500_mmu_translate(&c, p + 1, 0, 0);
                if (pp0 + 4 < MEMORY_SIZE) m0 = nd500_bus_read32(&m, pp0);
                if (pp1 + 4 < MEMORY_SIZE) m1 = nd500_bus_read32(&m, pp1);
            }
            log[ln % NLOG].ic = ic; log[ln % NLOG].pc = pc;
            log[ln % NLOG].a = p; log[ln % NLOG].b = m0; log[ln % NLOG].cval = m1;
            ln++;
        }
        if (pc == 0x08024183u || pc == 0x080241F4u) {
            /* at these PCs the frame/stack slot is mapped; safe to translate once. */
            unsigned long long ic = c.instruction_count;
            uint32_t Bnow = c.B, arg = c.I[0], slot = 0, pval = 0;
            if (pc == 0x080241F4u) {
                slot = Bnow + 0x14;
                if (c.machine && c.machine->mmu_enabled) {
                    uint32_t pp = nd500_mmu_translate(&c, slot, 0, 0);
                    if (pp + 4 < MEMORY_SIZE) pval = nd500_bus_read32(&m, pp);
                }
            }
            log[ln % NLOG].ic = ic; log[ln % NLOG].pc = pc;
            log[ln % NLOG].a = arg; log[ln % NLOG].b = Bnow; log[ln % NLOG].cval = pval;
            ln++;
        }
        nd500_cpu_step(&c);
        if (m.run_flag == 0 && m.stop_reason != STOP_NONE) break;
    }

    printf("---- last %d snapshots (of %d) ----\n", ln < NLOG ? ln : NLOG, ln);
    int start = ln > NLOG ? ln - NLOG : 0;
    for (int k = start; k < ln; k++) {
        int s = k % NLOG;
        if (log[s].pc == 0x08024180u)
            printf("  #%llu ARGGEN 0x08024180 p(I0)=%08X mem[p+0]=%08X mem[p+1]=%08X\n",
                   log[s].ic, log[s].a, log[s].b, log[s].cval);
        else if (log[s].pc == 0x08024183u)
            printf("  #%llu CALL 0x08024183  arg(I0)=%08X  B=%08X\n", log[s].ic, log[s].a, log[s].b);
        else
            printf("  #%llu PLOAD 0x080241F4  B=%08X  mem[B+0x14]=%08X\n", log[s].ic, log[s].b, log[s].cval);
    }
    printf("\nstop=%s instr#=%llu PC=%08X\n",
           nd500_stop_reason_str(m.stop_reason),
           (unsigned long long)c.instruction_count, c.PC);
    fflush(stdout);
    nd500_machine_free(&m);
    return 0;
}
