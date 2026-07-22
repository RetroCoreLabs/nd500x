/*
 * Diagnostic: ground-truth effective-address + slot-S dump at the NC crash.
 *
 * Static disassembly of 0x080241F9 (09 F5 00  h2 := r2.(0x0)) does not reconcile
 * with the observed fault address, so this harness stops the CPU exactly at the
 * crash instruction (before executing it) and dumps, from the emulator's OWN
 * decode + EA computation: op->reg, each operand's effective_address, the 32-bit
 * word actually read at that address, and all registers. That pins slot S.
 *
 * Then PASS B replays and logs every memory write whose value == TARGET, giving
 * the writer PC + instr#.
 *
 * Usage (run from build/nc_sandbox so A:C / B:C resolve):
 *   ND500X_PIN_CLOCK=1 ../bin/diag_crash_ea [cmd] [crash_pc_hex] [target_hex]
 * Defaults: cmd="COMPILE A,A,A\r", crash_pc=0x080241F9, target=0x4B6F7076
 *
 * Build (same static-lib link line as diag_r2_provenance.c):
 *   gcc -O2 -o build/bin/diag_crash_ea test/diag_crash_ea.c \
 *     -Wl,--start-group build/lib/libnd500_debugger.a build/lib/libnd500_cpu.a \
 *     build/lib/libnd500_machine.a build/lib/libmon.a build/lib/libnd500_ndlib.a \
 *     build/lib/libnd500_disasm.a -Wl,--end-group -lpthread -lm
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
static const char* CMD = "COMPILE A,A,A\r";

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

static int setup(Nd500Machine* m, Nd500Cpu* c) {
    nd500_machine_init(m, MEMORY_SIZE);
    if (!m->memory) return 2;
    nd500_cpu_init(c, m);
    nd500_cpu_reset(c);
    nd500_mmu_init(c);
    nd500_domain_init(c);
    mon_init();
    prime_input();
    uint32_t sa = 0; int dom = 0;
    if (ndlib_dom_load_to_machine(m, c, -1, NULL, NULL, &sa, &dom) != 0) return 2;
    m->run_flag = 1; m->stop_reason = STOP_NONE;
    return 0;
}

static void dump_around(Nd500Machine* m, uint32_t center, int before, int after) {
    long start = (long)center - before, end = (long)center + after;
    for (long a = start; a < end; a += 16) {
        printf("  %08lX: ", (unsigned long)a);
        char asc[17]; asc[16] = 0;
        for (int i = 0; i < 16; i++) {
            uint8_t b = nd500_bus_read8(m, (uint32_t)(a + i));
            printf("%02X ", b);
            asc[i] = (b >= 0x20 && b < 0x7F) ? (char)b : '.';
        }
        printf(" |%s|\n", asc);
    }
}

int main(int argc, char** argv) {
    if (argc > 1) CMD = argv[1];
    uint32_t CRASH_PC = (argc > 2) ? (uint32_t)strtoul(argv[2], 0, 0) : 0x080241F9u;
    uint32_t TARGET   = (argc > 3) ? (uint32_t)strtoul(argv[3], 0, 0) : 0x4B6F7076u;

    if (ndlib_load_dom_header(DOM) != 0) { fprintf(stderr, "header load failed\n"); return 2; }
    if (ndlib_load_dom_segments() != 0)  { fprintf(stderr, "segment load failed\n"); return 2; }

    /* ---------- PASS A: stop at crash PC, dump ground-truth EA ---------- */
    Nd500Machine ma; Nd500Cpu ca;
    if (setup(&ma, &ca) != 0) { fprintf(stderr, "pass A setup failed\n"); return 2; }

    /* CRASH_PC is inside a linked-list walk executed many times. Log the register
     * state at EVERY execution of CRASH_PC so we can see how I[1] evolves into the
     * bad value. Keep a ring of the last N hits and print them after the fault. */
    #define RING 24
    struct { unsigned long long ic; uint32_t i0,i1,i2,i3,ea; } ring[RING];
    int rn = 0; long step = 0; int stopped_at_crash = 0;
    for (; step < MAX_STEPS && ma.run_flag; step++) {
        if (ca.PC == CRASH_PC) {
            Nd500FetchedInstruction pfi; memset(&pfi, 0, sizeof(pfi));
            nd500_decode_at(&ma, ca.PC, &pfi);
            uint32_t ea = pfi.operand_count ? pfi.operands[pfi.operand_count-1].effective_address : 0;
            ring[rn % RING].ic = ca.instruction_count;
            ring[rn % RING].i0 = ca.I[0]; ring[rn % RING].i1 = ca.I[1];
            ring[rn % RING].i2 = ca.I[2]; ring[rn % RING].i3 = ca.I[3];
            ring[rn % RING].ea = ea; rn++;
        }
        nd500_cpu_step(&ca);
        if (ma.run_flag == 0 && ma.stop_reason != STOP_NONE) { stopped_at_crash = 1; break; }
    }
    printf("---- last %d executions of 0x%08X (instr#, I1..I4, EA) ----\n",
           rn < RING ? rn : RING, CRASH_PC);
    int start = rn > RING ? rn - RING : 0;
    for (int k = start; k < rn; k++) {
        int s = k % RING;
        printf("  #%llu  I1=%08X I2=%08X I3=%08X I4=%08X  EA=%08X\n",
               ring[s].ic, ring[s].i0, ring[s].i1, ring[s].i2, ring[s].i3, ring[s].ea);
    }
    fflush(stdout);
    nd500_machine_free(&ma);
    return 0;

    if (!stopped_at_crash) {
        fprintf(stderr, "PASS A: never reached crash PC 0x%08X (steps=%ld stop=%s)\n",
                CRASH_PC, step, nd500_stop_reason_str(ma.stop_reason));
        nd500_machine_free(&ma);
        return 1;
    }

    printf("==================== GROUND TRUTH @ crash PC ====================\n");
    printf("instr#: %llu  PC: 0x%08X\n", (unsigned long long)ca.instruction_count, ca.PC);
    printf("regs: B=%08X R=%08X L=%08X TOS=%08X\n", ca.B, ca.R, ca.L, ca.TOS);
    printf("      I1=%08X I2=%08X I3=%08X I4=%08X\n", ca.I[0], ca.I[1], ca.I[2], ca.I[3]);
    printf("      A1=%08X A2=%08X A3=%08X A4=%08X\n", ca.A[0], ca.A[1], ca.A[2], ca.A[3]);

    Nd500FetchedInstruction fi; memset(&fi, 0, sizeof(fi));
    nd500_decode_at(&ma, ca.PC, &fi);
    char dis[256] = {0};
    nd500_disasm_format_range(&ma, ca.PC, fi.total_len ? fi.total_len : 3, dis, sizeof(dis));
    printf("disasm: %s\n", dis);
    printf("operand_count: %d\n", fi.operand_count);
    uint32_t slotS = 0; int haveS = 0;
    for (int i = 0; i < fi.operand_count; i++) {
        int mode = (int)fi.operands[i].mode;
        uint32_t ea = fi.operands[i].effective_address;
        printf("  op[%d] mode=%d reg=%d acode=0x%02X  EA=0x%08X",
               i, mode, fi.operands[i].reg, fi.operands[i].address_code, ea);
        if (mode != ND500_ADDR_REGISTER && mode != ND500_ADDR_CONSTANT &&
            mode != ND500_ADDR_CONSTANT_SHORT) {
            uint32_t w = nd500_bus_read32(&ma, ea);
            printf("  word@EA=0x%08X", w);
            if (w == TARGET) { slotS = ea; haveS = 1; }
        }
        printf("\n");
    }
    printf("\n---- 64 bytes around EA of op[last memory operand] ----\n");
    if (fi.operand_count > 0) {
        uint32_t ea = fi.operands[fi.operand_count - 1].effective_address;
        dump_around(&ma, ea, 32, 32);
        if (!haveS) { slotS = ea; haveS = 1; }
    }
    fflush(stdout);
    nd500_machine_free(&ma);

    if (!haveS) { printf("\nNo slot S identified.\n"); return 0; }

    /* ---------- PASS B: who writes TARGET into slot S ---------- */
    Nd500Machine mb; Nd500Cpu cb;
    if (setup(&mb, &cb) != 0) { fprintf(stderr, "pass B setup failed\n"); return 2; }
    printf("\n==================== WRITER of slot S=0x%08X ====================\n", slotS);
    uint32_t prev = nd500_bus_read32(&mb, slotS);
    long sb = 0; int found = 0;
    unsigned long long w_no = 0; uint32_t w_pc = 0;
    for (; sb < MAX_STEPS && mb.run_flag; sb++) {
        unsigned long long ic = cb.instruction_count; uint32_t pc = cb.PC;
        nd500_cpu_step(&cb);
        uint32_t w = nd500_bus_read32(&mb, slotS);
        if (w == TARGET && prev != TARGET) { w_no = ic; w_pc = pc; found = 1; break; }
        prev = w;
        if (mb.run_flag == 0 && mb.stop_reason != STOP_NONE) break;
    }
    if (!found) {
        printf("No write of 0x%08X to S observed (word@S end=0x%08X stop=%s)\n",
               TARGET, nd500_bus_read32(&mb, slotS), nd500_stop_reason_str(mb.stop_reason));
    } else {
        Nd500FetchedInstruction wfi; memset(&wfi, 0, sizeof(wfi));
        nd500_decode_at(&mb, w_pc, &wfi);
        char wd[256] = {0};
        nd500_disasm_format_range(&mb, w_pc, wfi.total_len ? wfi.total_len : 3, wd, sizeof(wd));
        printf("writer instr#: %llu\n", w_no);
        printf("writer PC    : 0x%08X\n", w_pc);
        printf("writer disasm: %s\n", wd);
        printf("\n---- 64 bytes around S at write time ----\n");
        dump_around(&mb, slotS, 32, 32);
    }
    fflush(stdout);
    nd500_machine_free(&mb);
    return 0;
}
