/*
 * Diagnostic: Provenance of the value 0x54312D42 ("T1-B") that lands in the
 * ND-500 index register I2 (disassembled as "r2" in PREINDEXED mode) and
 * causes the deterministic NC crash at 0x080241F9 (09 F5 00  h2 := r2.(0x0)).
 *
 * PASS A (Q1): step NC to the crash region; detect the FIRST instruction that
 *   loads cpu.I[1] with 0x54312D42. Decode that instruction (EA computed from
 *   the pre-step register state) and, if it is a memory load, capture the
 *   source effective address S (virtual) and its physical translation P.
 *
 * PASS B (Q2): re-load NC from scratch; on every step poll the 32-bit word at
 *   physical P. When it first becomes 0x54312D42, report the instr#, PC and a
 *   64-byte hex+ASCII dump around P at that instant.
 *
 * Driving of NC is copied verbatim from test/diag_nc_checkpoints.c
 * (ND500X_PIN_CLOCK deterministic clock, COMPILE A,A,A console input).
 *
 * Build (static-lib link line, same as diag_nc_checkpoints.c):
 *   gcc -O2 -o build/bin/diag_r2_provenance test/diag_r2_provenance.c \
 *     -Wl,--start-group build/lib/libnd500_debugger.a build/lib/libnd500_cpu.a \
 *     build/lib/libnd500_machine.a build/lib/libmon.a build/lib/libnd500_ndlib.a \
 *     build/lib/libnd500_disasm.a -Wl,--end-group -lpthread -lm
 * Run:
 *   ND500X_PIN_CLOCK=1 ./build/bin/diag_r2_provenance
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
static uint32_t TARGET = 0x54312D42u;   /* overridable via argv[1] */
static int      REG    = 1;             /* index into cpu.I[]; overridable via argv[2] */

static const char* DOM = "/mnt/d/ND/500/FraTor/nc/nc-a06.dom";

static void prime_input(void) {
    const char* cmd = "COMPILE A,A,A\r";
    char inbuf[128];
    size_t j = 0;
    for (size_t i = 0; cmd[i] && j < sizeof(inbuf) - 2; i++) {
        if (cmd[i] == '\\' && cmd[i + 1] == 'r') { inbuf[j++] = '\r'; i++; }
        else if (cmd[i] == ';' && cmd[i + 1] == ';') { inbuf[j++] = '\r'; i++; }
        else inbuf[j++] = cmd[i];
    }
    inbuf[j] = 0;
    mon_queue_console_input(inbuf);
}

/* Load NC into a fresh machine/cpu. Returns 0 on success. */
static int setup(Nd500Machine* machine, Nd500Cpu* cpu) {
    nd500_machine_init(machine, MEMORY_SIZE);
    if (!machine->memory) return 2;
    nd500_cpu_init(cpu, machine);
    nd500_cpu_reset(cpu);
    nd500_mmu_init(cpu);
    nd500_domain_init(cpu);
    mon_init();
    prime_input();
    uint32_t start_addr = 0; int domain = 0;
    if (ndlib_dom_load_to_machine(machine, cpu, -1, NULL, NULL,
                                  &start_addr, &domain) != 0) return 2;
    machine->run_flag = 1;
    machine->stop_reason = STOP_NONE;
    return 0;
}

static void dump_around(Nd500Machine* m, uint32_t center, int before, int after) {
    long start = (long)center - before;
    long end   = (long)center + after;
    for (long a = start; a < end; a += 16) {
        printf("  %08lX: ", (unsigned long)a);
        char ascii[17]; ascii[16] = 0;
        for (int i = 0; i < 16; i++) {
            uint8_t b = nd500_bus_read8(m, (uint32_t)(a + i));
            printf("%02X ", b);
            ascii[i] = (b >= 0x20 && b < 0x7F) ? (char)b : '.';
        }
        printf(" |%s|\n", ascii);
    }
}

int main(int argc,char**argv){ if(argc>1)TARGET=(uint32_t)strtoul(argv[1],0,0); if(argc>2)REG=atoi(argv[2]);
    /* ---- header/segments loaded once (ndlib globals), reused by both passes ---- */
    if (ndlib_load_dom_header(DOM) != 0) { fprintf(stderr, "header load failed\n"); return 2; }
    if (ndlib_load_dom_segments() != 0)  { fprintf(stderr, "segment load failed\n"); return 2; }

    /* ================= PASS A : Q1 provenance of I[1] ================= */
    Nd500Machine ma; Nd500Cpu ca;
    if (setup(&ma, &ca) != 0) { fprintf(stderr, "pass A setup failed\n"); return 2; }
    fprintf(stderr, "PASS A start PC=0x%08X clock=%s\n", ca.PC,
            mon_clock_is_deterministic() ? "PINNED" : "REAL(!)");

    Nd500Cpu snap;                 /* pre-step register snapshot */
    uint32_t hit_instr_pc = 0;
    unsigned long long hit_instr_no = 0;
    uint32_t src_ea = 0, src_pa = 0; int found_q1 = 0; int src_is_load = 0;
    char disbuf[256] = {0};
    long step = 0;
    uint32_t prev_i1 = ca.I[REG];

    for (; step < MAX_STEPS && ma.run_flag; step++) {
        memcpy(&snap, &ca, sizeof(Nd500Cpu));   /* pre-step state (for EA calc) */
        unsigned long long ic = ca.instruction_count;
        uint32_t pc = ca.PC;

        nd500_cpu_step(&ca);

        if (ca.I[REG] == TARGET && prev_i1 != TARGET) {
            /* This just-executed instruction set I[REG] to the target value.
             * Record it; keep the LAST such event before the run stops (the
             * load that is live at the crash), so do NOT break here. */
            src_is_load = 0; src_ea = 0; src_pa = 0;
            hit_instr_no = ic;
            hit_instr_pc = pc;
            /* Decode using pre-step register state so EA matches. */
            Nd500Cpu* saved = ma.cpu;
            ma.cpu = &snap;
            Nd500FetchedInstruction fi;
            memset(&fi, 0, sizeof(fi));
            nd500_decode_at(&ma, pc, &fi);
            nd500_disasm_format_range(&ma, pc, fi.total_len ? fi.total_len : 3,
                                      disbuf, sizeof(disbuf));
            /* Find a memory-source operand (skip pure REGISTER/CONSTANT). */
            for (int i = 0; i < fi.operand_count; i++) {
                Nd500AddrMode md = fi.operands[i].mode;
                if (md == ND500_ADDR_REGISTER || md == ND500_ADDR_CONSTANT ||
                    md == ND500_ADDR_CONSTANT_SHORT || md == ND500_ADDR_UNKNOWN)
                    continue;
                src_ea = fi.operands[i].effective_address;
                src_is_load = 1;
                break;
            }
            if (src_is_load) {
                /* translate S -> physical P in the (pre-step) crash context */
                if (snap.machine && snap.machine->mmu_enabled)
                    src_pa = nd500_mmu_translate(&snap, src_ea, 0, 0);
                else
                    src_pa = src_ea;
            }
            found_q1 = 1;
            ma.cpu = saved;
            /* no break: keep the most recent occurrence */
        }
        prev_i1 = ca.I[REG];
        if (ma.run_flag == 0 && ma.stop_reason != STOP_NONE) break;
    }

    if (!found_q1) {
        fprintf(stderr, "PASS A: I[1] never became 0x%08X (steps=%ld, stop=%s)\n",
                TARGET, step, nd500_stop_reason_str(ma.stop_reason));
        nd500_machine_free(&ma);
        return 1;
    }

    printf("==================== Q1 (provenance / load) ====================\n");
    printf("instr#      : %llu\n", hit_instr_no);
    printf("PC          : 0x%08X\n", hit_instr_pc);
    printf("disasm      : %s\n", disbuf);
    printf("target field: cpu.I[1]  (I2 index register; \"r2\" in PREINDEXED)\n");
    printf("value        : 0x%08X (\"T1-B\")\n", TARGET);
    if (src_is_load) {
        printf("kind         : MEMORY LOAD\n");
        printf("source EA S  : 0x%08X (virtual)\n", src_ea);
        printf("physical  P  : 0x%08X\n", src_pa);
    } else {
        printf("kind         : COMPUTED (no memory-source operand found)\n");
    }
    printf("\n---- 64 bytes around source EA S (virtual, current PASS-A context) ----\n");
    if (src_is_load) dump_around(&ma, src_ea, 32, 32);
    fflush(stdout);

    uint32_t P = src_pa;
    int have_P = src_is_load;
    nd500_machine_free(&ma);

    if (!have_P) {
        printf("\nQ1 was a computed value; no memory slot S to trace for Q2.\n");
        return 0;
    }

    /* ================= PASS B : Q2 who wrote physical P ================= */
    Nd500Machine mb; Nd500Cpu cb;
    if (setup(&mb, &cb) != 0) { fprintf(stderr, "pass B setup failed\n"); return 2; }
    fprintf(stderr, "PASS B start PC=0x%08X polling phys P=0x%08X\n", cb.PC, P);

    unsigned long long w_instr_no = 0; uint32_t w_pc = 0; int found_q2 = 0;
    uint32_t prev_word = nd500_bus_read32(&mb, P);
    long stepB = 0;
    for (; stepB < MAX_STEPS && mb.run_flag; stepB++) {
        unsigned long long ic = cb.instruction_count;
        uint32_t pc = cb.PC;
        nd500_cpu_step(&cb);
        uint32_t w = nd500_bus_read32(&mb, P);
        if (w == TARGET && prev_word != TARGET) {
            w_instr_no = ic; w_pc = pc; found_q2 = 1;
            break;
        }
        prev_word = w;
        if (mb.run_flag == 0 && mb.stop_reason != STOP_NONE) break;
    }

    printf("\n==================== Q2 (writer of slot S) ====================\n");
    if (!found_q2) {
        printf("No step transitioned phys P=0x%08X to 0x%08X.\n", P, TARGET);
        printf("(word at P at end = 0x%08X, stop=%s)\n",
               nd500_bus_read32(&mb, P), nd500_stop_reason_str(mb.stop_reason));
    } else {
        char wdis[256] = {0};
        Nd500FetchedInstruction wfi; memset(&wfi, 0, sizeof(wfi));
        nd500_decode_at(&mb, w_pc, &wfi);
        nd500_disasm_format_range(&mb, w_pc, wfi.total_len ? wfi.total_len : 3,
                                  wdis, sizeof(wdis));
        printf("instr#      : %llu\n", w_instr_no);
        printf("PC          : 0x%08X\n", w_pc);
        printf("disasm      : %s\n", wdis);
        printf("wrote 0x%08X to phys P=0x%08X\n", TARGET, P);
        printf("\n---- 64 bytes around phys P at write time ----\n");
        dump_around(&mb, P, 32, 32);
    }
    fflush(stdout);
    nd500_machine_free(&mb);
    return 0;
}
