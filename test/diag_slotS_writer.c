/*
 * diag_slotS_writer.c - Root-cause forensics for the NC self-walking-list crash.
 *
 * The crash instruction 0x080241F9 (h2 := r2.(0x0)) walks a linked list:
 * each visit does I[1] := mem[I[1]+0]. The fatal I[1] (a name, e.g. 0x4B6F7076)
 * was loaded on the PREVIOUS visit from slot S = that visit's EA. This harness:
 *
 * PASS A: at TOP of every 0x080241F9 execution (before nd500_cpu_step, so trap
 *   masking cannot hide the value), record instr#, I[1], EA, and word32@EA.
 *   Slot S = the EA of the visit whose word32@EA == TARGET. Also translate S to
 *   physical, and scan all physical RAM for the 4-byte TARGET pattern.
 *
 * PASS B: fresh run; each step poll word32 at virtual S. Report the FIRST write
 *   that makes it == TARGET: instr#, writer PC, disasm, 64-byte dump around S.
 *
 * Usage (MUST run from build/nc_sandbox):
 *   ND500X_PIN_CLOCK=1 ../bin/diag_slotS_writer "COMPILE A,A,A\r" 0x080241F9 0x4B6F7076
 *   ND500X_PIN_CLOCK=1 ../bin/diag_slotS_writer "CHECK B,B,B\r"   0x080241FC 0x54312D42
 *
 * Build:
 *   gcc -O2 -o build/bin/diag_slotS_writer test/diag_slotS_writer.c \
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
#include "testdata.h"

#define MEMORY_SIZE   (16u * 1024u * 1024u)
#define MAX_STEPS     2000000L

static const char* DOM; /* assigned in main - nd500_testdata() is a call, not a constant */
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
    DOM = nd500_testdata("FraTor/nc/nc-a06.dom");
    if (argc > 1) CMD = argv[1];
    uint32_t TARGET   = (argc > 3) ? (uint32_t)strtoul(argv[3], 0, 0) : 0x4B6F7076u;

    if (ndlib_load_dom_header(DOM) != 0) { fprintf(stderr, "header load failed\n"); return 2; }
    if (ndlib_load_dom_segments() != 0)  { fprintf(stderr, "segment load failed\n"); return 2; }

    /* ---------------- PASS A: ring of every crash-PC visit ---------------- */
    Nd500Machine ma; Nd500Cpu ca;
    if (setup(&ma, &ca) != 0) { fprintf(stderr, "pass A setup failed\n"); return 2; }

    #define RING 40
    /* Ring of instructions that CHANGE I[1]: capture PC (of the changing instr),
     * new I[1], decoded source-operand EA and the word read there. */
    struct { unsigned long long ic; uint32_t pc, newv, srcea, srcword; int has_src; char dis[96]; } ring[RING];
    int rn = 0; long step = 0;
    uint32_t prev_i1 = ca.I[1];
    #define PCRING 20
    struct { unsigned long long ic; uint32_t pc, i1, b, ea; char dis[80]; } pcr[PCRING];
    int pn = 0;
    for (; step < MAX_STEPS && ma.run_flag; step++) {
        uint32_t pc = ca.PC;
        { /* full-instruction PC ring (top-of-instruction state) */
            Nd500FetchedInstruction fpc; memset(&fpc, 0, sizeof(fpc));
            nd500_decode_at(&ma, pc, &fpc);
            int q = pn % PCRING;
            pcr[q].ic = ca.instruction_count; pcr[q].pc = pc; pcr[q].i1 = ca.I[1]; pcr[q].b = ca.B;
            pcr[q].ea = fpc.operand_count ? fpc.operands[fpc.operand_count-1].effective_address : 0;
            nd500_disasm_format_range(&ma, pc, fpc.total_len ? fpc.total_len : 3, pcr[q].dis, sizeof(pcr[q].dis));
            pn++;
        }
        /* pre-step snapshot for EA calc */
        Nd500Cpu snap; memcpy(&snap, &ca, sizeof(Nd500Cpu));
        nd500_cpu_step(&ca);
        if (ca.I[1] != prev_i1) {
            Nd500Cpu* saved = ma.cpu; ma.cpu = &snap;
            Nd500FetchedInstruction fi; memset(&fi, 0, sizeof(fi));
            nd500_decode_at(&ma, pc, &fi);
            int s = rn % RING;
            ring[s].ic = snap.instruction_count; ring[s].pc = pc; ring[s].newv = ca.I[1];
            ring[s].has_src = 0; ring[s].srcea = 0; ring[s].srcword = 0;
            for (int i = 0; i < fi.operand_count; i++) {
                Nd500AddrMode md = fi.operands[i].mode;
                if (md == ND500_ADDR_REGISTER || md == ND500_ADDR_CONSTANT ||
                    md == ND500_ADDR_CONSTANT_SHORT || md == ND500_ADDR_UNKNOWN) continue;
                ring[s].srcea = fi.operands[i].effective_address; ring[s].has_src = 1;
                ring[s].srcword = nd500_bus_read32(&ma, ring[s].srcea); break;
            }
            nd500_disasm_format_range(&ma, pc, fi.total_len ? fi.total_len : 3, ring[s].dis, sizeof(ring[s].dis));
            ma.cpu = saved;
            rn++;
        }
        prev_i1 = ca.I[1];
        if (ma.run_flag == 0 && ma.stop_reason != STOP_NONE) break;
    }

    printf("==================== PASS A: last %d changes to I[1] ====================\n", RING);
    printf("stop=%s  total I[1] changes=%d\n", nd500_stop_reason_str(ma.stop_reason), rn);
    int start = rn > RING ? rn - RING : 0;
    uint32_t slotS = 0; unsigned long long slotS_ic = 0; int haveS = 0; uint32_t realval = 0;
    for (int k = start; k < rn; k++) {
        int s = k % RING;
        printf("  #%llu PC=%08X newI1=%08X  src=%s@%08X word@src=%08X  %s\n",
               ring[s].ic, ring[s].pc, ring[s].newv,
               ring[s].has_src ? "M" : "-", ring[s].srcea, ring[s].srcword, ring[s].dis);
    }
    /* The LAST I[1] change before the fault is the load that placed the bad value.
     * Its source EA is slot S; its loaded value is the real name stored there. */
    /* The very last change is the faulting deref (r2.2). The one before it is the
     * load of the NAME into I[1] from slot S. Use the 2nd-to-last as S. */
    if (rn >= 2) {
        int s = (rn - 2) % RING;
        if (ring[s].has_src) { slotS = ring[s].srcea; slotS_ic = ring[s].ic; realval = ring[s].newv; haveS = 1; }
        printf("\nNAME-LOAD (2nd-to-last I[1] change): instr#%llu PC=%08X  loaded 0x%08X from %s@%08X\n",
               ring[s].ic, ring[s].pc, ring[s].newv, ring[s].has_src?"mem":"reg", ring[s].srcea);
    }
    TARGET = realval;   /* trace the actual stored value, not the fault-address+2 */

    printf("\n---- final instruction chain (top-of-instruction state) ----\n");
    int pstart = pn > PCRING ? pn - PCRING : 0;
    for (int k = pstart; k < pn; k++) {
        int q = k % PCRING;
        printf("  instr#%llu PC=%08X I1=%08X B=%08X EA=%08X  %s\n",
               pcr[q].ic, pcr[q].pc, pcr[q].i1, pcr[q].b, pcr[q].ea, pcr[q].dis);
    }

    if (!haveS) {
        printf("\nTARGET-setting instr had no memory-source operand (computed value); no slot S from load.\n");
    } else {
        printf("\nSLOT S (virtual) = 0x%08X  (source of the load that set I[1]=TARGET at instr#%llu)\n", slotS, slotS_ic);
        uint32_t phys = ma.mmu_enabled ? nd500_mmu_translate(&ca, slotS, 0, 0) : slotS;
        printf("SLOT S (physical, MMU-translated) = 0x%08X\n", phys);
        printf("word32@S now = 0x%08X\n", nd500_bus_read32(&ma, slotS));
        printf("\n---- 64 bytes around S (virtual) at crash time ----\n");
        dump_around(&ma, slotS, 32, 32);
    }

    /* Physical RAM scan for the TARGET byte pattern (big-endian order). Collect hits. */
    uint32_t hitlist[32]; int nhit = 0;
    {
        uint8_t p0=(TARGET>>24)&0xFF,p1=(TARGET>>16)&0xFF,p2=(TARGET>>8)&0xFF,p3=TARGET&0xFF;
        printf("\n---- RAM scan pattern %02X %02X %02X %02X (big-endian) ----\n", p0,p1,p2,p3);
        for (uint32_t p=0; p+4<=ma.memory_size; p++) {
            if (ma.memory[p]==p0 && ma.memory[p+1]==p1 && ma.memory[p+2]==p2 && ma.memory[p+3]==p3) {
                printf("  phys 0x%08X\n", p); if (nhit < 32) hitlist[nhit++] = p;
            }
        }
        if (!nhit) printf("  (no match)\n");
        for (int i=0;i<nhit;i++){
            printf("  context @phys 0x%08X:\n", hitlist[i]);
            long c=(long)hitlist[i];
            for (long a=c-16;a<c+32;a+=16){ printf("    %08lX: ",(unsigned long)a);
              char asc[17]; asc[16]=0; for(int k=0;k<16;k++){uint8_t b=ma.memory[a+k]; printf("%02X ",b);
              asc[k]=(b>=0x20&&b<0x7F)?(char)b:'.';} printf(" |%s|\n",asc);}
        }
    }
    fflush(stdout);
    uint32_t physP = ma.mmu_enabled ? nd500_mmu_translate(&ca, slotS, 0, 0) : slotS;
    nd500_machine_free(&ma);

    if (!haveS) { printf("\nNo slot S -> cannot run writer pass.\n"); return 0; }
    (void)physP;

    /* ---------------- PASS B: first writer of the name pattern at each scan-hit ---------------- */
    Nd500Machine mb; Nd500Cpu cb;
    if (setup(&mb, &cb) != 0) { fprintf(stderr, "pass B setup failed\n"); return 2; }
    printf("\n==================== PASS B: first writer of name-pattern at %d phys slots ====================\n", nhit);
    uint8_t t0=(TARGET>>24)&0xFF,t1=(TARGET>>16)&0xFF,t2=(TARGET>>8)&0xFF,t3=TARGET&0xFF;
    struct { int done; unsigned long long ic; uint32_t pc; } W[32];
    for (int i=0;i<nhit;i++){ W[i].done=0; W[i].ic=0; W[i].pc=0; }
    #define MATCHP(mm,p) ((mm).memory[(p)]==t0&&(mm).memory[(p)+1]==t1&&(mm).memory[(p)+2]==t2&&(mm).memory[(p)+3]==t3)
    int alldone=0; long sb=0;
    for (; sb < MAX_STEPS && mb.run_flag && !alldone; sb++) {
        unsigned long long ic = cb.instruction_count; uint32_t pc = cb.PC;
        nd500_cpu_step(&cb);
        alldone=1;
        for (int i=0;i<nhit;i++){
            if (W[i].done) continue;
            if (MATCHP(mb, hitlist[i])) { W[i].done=1; W[i].ic=ic; W[i].pc=pc; }
            else alldone=0;
        }
        if (mb.run_flag == 0 && mb.stop_reason != STOP_NONE) break;
    }
    printf("stop=%s  steps=%ld\n", nd500_stop_reason_str(mb.stop_reason), sb);
    for (int i=0;i<nhit;i++){
        printf("\n-- phys slot 0x%08X --\n", hitlist[i]);
        if (!W[i].done) { printf("   never matched name pattern during PASS B\n"); continue; }
        Nd500FetchedInstruction wfi; memset(&wfi,0,sizeof(wfi));
        nd500_decode_at(&mb, W[i].pc, &wfi);
        char wd[256]={0};
        nd500_disasm_format_range(&mb, W[i].pc, wfi.total_len?wfi.total_len:3, wd, sizeof(wd));
        printf("   first-writer instr#%llu PC=0x%08X  %s\n", W[i].ic, W[i].pc, wd);
        long c=(long)hitlist[i];
        for (long a=c-16; a<c+16; a+=16){ printf("   %08lX: ",(unsigned long)a);
          char asc[17]; asc[16]=0; for(int k=0;k<16;k++){uint8_t b=mb.memory[a+k]; printf("%02X ",b);
          asc[k]=(b>=0x20&&b<0x7F)?(char)b:'.';} printf(" |%s|\n",asc);}
    }
    fflush(stdout);
    nd500_machine_free(&mb);
    return 0;
}
