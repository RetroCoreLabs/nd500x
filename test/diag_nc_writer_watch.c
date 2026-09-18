/*
 * Diagnostic: NC codegen crash 0x08023EA4 - writer/reader register capture.
 *
 * Mirrors the C# TestNC_CodegenCrashWatch approach on the C side. Drives the
 * real NC compiler (nc-a06.dom) to the code-generation crash and, WITHOUT
 * probing memory (so the run is not perturbed), logs the CPU registers at:
 *
 *   - the WRITER  PC 0x0802CEFE  (w1 =: r2.0) whenever it stores into the
 *     seg-3 root region 0x18003000..0x1800300F, and
 *   - the READER  PC 0x08023EA1  (by test r1.0) which dereferences the loaded
 *     link and faults on the wild pointer.
 *
 * Purpose: answer the open question "is the value already corrupt when it
 * reaches the store (upstream pack), or does the store corrupt a clean
 * pointer?" - by showing whether the value being stored (in a Wn register at
 * the writer) is already the wild pattern before the store executes.
 *
 * Build: standalone against the built static libs (see the gcc line in
 * docs/DAP_BUG_REPORTS.md / this file's header). Not wired into ctest.
 *
 * Run from the NC sandbox dir (build/nc_sandbox) so the SINTRAN source files
 * A:C etc. resolve.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/instruction_helpers.h"   /* nd500_read_memory_32 */
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/cpu/nd500_domain.h"
#include "../src/ndlib/ndlib.h"
#include <ndmon/mon.h>
#include <ndmon/mon_file_table.h>
#include "testdata.h"

#define MEMORY_SIZE   (16u * 1024u * 1024u)
#define WRITER_PC     0x0802CEFEu
#define WALK_PC       0x08023E9Eu   /* w1 := r1.2 : R1(pre) = source node */
#define READER_PC     0x08023EA1u
#define STORE884_PC   0x08024884u   /* w1 =: IND(b.52)(r2): target=*(B+0x34)+R2 */
#define ROOT_LO       0x18003000u
#define ROOT_HI       0x18003010u

/* Walk NC's node free-list (head at 0x0801D544; each free element's first word
 * = next free node, 0 = end) and report whether `target` is a member. Prints the
 * chain (bounded) so its structure is visible. Non-perturbing (clears traps). */
#define FREELIST_HEAD 0x0801D544u
static void dump_freelist(Nd500Cpu* cpu, uint32_t target, const char* when) {
    uint32_t node = nd500_read_memory_32(cpu, FREELIST_HEAD);
    nd500_trap_clear();
    printf("[FREELIST %s] head(0x%08X)=0x%08X chain:", when, FREELIST_HEAD, node);
    int found = 0, n = 0;
    uint32_t seen_first = node;
    while (node != 0 && n < 40) {
        printf(" 0x%08X", node);
        if (node == target) { found = 1; printf("<==TARGET"); }
        uint32_t next = nd500_read_memory_32(cpu, node);  /* node[+0] = next free */
        nd500_trap_clear();
        node = next;
        n++;
        if (node == seen_first) { printf(" ...(cycle)"); break; }
    }
    printf("\n[FREELIST %s] target 0x%08X %s the free-list\n",
           when, target, found ? "IS ON" : "is NOT on");
}

int main(int argc, char** argv) {
    const char* dom = (argc > 1) ? argv[1] : nd500_testdata("FraTor/nc/nc-a06.dom");
    const char* cmd = (argc > 2) ? argv[2] : "COMPILE A,A,A\r";
    long max_steps  = (argc > 3) ? strtol(argv[3], NULL, 0) : 2000000L;

    if (ndlib_load_dom_header(dom) != 0) { printf("header load failed\n"); return 2; }
    if (ndlib_load_dom_segments() != 0)  { printf("segment load failed\n"); return 2; }

    Nd500Machine machine;
    nd500_machine_init(&machine, MEMORY_SIZE);
    if (!machine.memory) { printf("machine init failed\n"); return 2; }

    Nd500Cpu cpu;
    nd500_cpu_init(&cpu, &machine);
    nd500_cpu_reset(&cpu);
    nd500_mmu_init(&cpu);
    nd500_domain_init(&cpu);
    mon_init();

    /* Queue the compile command (process \r) */
    char inbuf[64];
    size_t j = 0;
    for (size_t i = 0; cmd[i] && j < sizeof(inbuf) - 1; i++) {
        if (cmd[i] == '\\' && cmd[i + 1] == 'r') { inbuf[j++] = '\r'; i++; }
        else inbuf[j++] = cmd[i];
    }
    inbuf[j] = 0;
    mon_queue_console_input(inbuf);

    uint32_t start_addr = 0; int domain = 0;
    if (ndlib_dom_load_to_machine(&machine, &cpu, -1, NULL, NULL,
                                  &start_addr, &domain) != 0) {
        printf("dom load-to-machine failed\n"); return 2;
    }
    printf("Loaded. start PC=0x%08X domain=%d. Watching writer 0x%08X, reader 0x%08X\n",
           cpu.PC, domain, WRITER_PC, READER_PC);

    machine.run_flag = 1;
    machine.stop_reason = STOP_NONE;

    /* Poll-watch the corrupted node (default 0x1802A1B0, override via argv[4]).
     * Log every change of its first word with the instruction that caused it. */
    uint32_t watch = (argc > 4) ? (uint32_t)strtoul(argv[4], NULL, 0) : 0x1802A1B0u;
    uint32_t watch_prev = 0; int watch_seen = 0;

    int writer_hits = 0;
    int g_cur880 = 0, g_cur884 = 0;
    long step = 0;
    uint32_t prev_pc = cpu.PC;   /* PC of the instruction that executed last */
    for (; step < max_steps && machine.run_flag; step++) {
        uint32_t pc = cpu.PC;

        /* Poll the watched node between ~allocation and the crash window. The
         * change was caused by the instruction that executed in the PREVIOUS
         * iteration (prev_pc), not by `pc` which is about to execute. */
        if (cpu.instruction_count > 1000000ull && cpu.instruction_count < 1493000ull) {
            uint32_t cur = nd500_read_memory_32(&cpu, watch);
            nd500_trap_clear();
            if (!watch_seen) { watch_prev = cur; watch_seen = 1; }
            else if (cur != watch_prev) {
                printf("[NODE-WRITE] instr=%llu writer_PC=0x%08X node[0x%08X] "
                       "0x%08X -> 0x%08X\n",
                       (unsigned long long)cpu.instruction_count - 1, prev_pc, watch,
                       watch_prev, cur);
                watch_prev = cur;
            }
        }

        /* CURSOR TABLE for the C# cross-diff. 0x08024884 is `w1 =: @b.0x34+`
         * (local indirect post-increment): target = *(B+0x34), value = I1, then
         * *(B+0x34) is post-incremented. 0x08024880 sets the cursor base from I2.
         * Log the FIRST ~15 executions of each (apples-to-apples anchor). */
        if (pc == 0x08024880u && g_cur880 < 15) {
            printf("[CUR880 #%d] instr=%llu B=0x%08X I2(cursor base -> B+0x34)=0x%08X\n",
                   g_cur880++, (unsigned long long)cpu.instruction_count, cpu.B, cpu.I[1]);
        }
        if (pc == STORE884_PC && g_cur884 < 15) {
            uint32_t b34 = nd500_read_memory_32(&cpu, cpu.B + 0x34);
            nd500_trap_clear();
            printf("[CUR884 #%d] instr=%llu B=0x%08X *(B+0x34)=0x%08X(target) I1(stored)=0x%08X\n",
                   g_cur884++, (unsigned long long)cpu.instruction_count, cpu.B, b34, cpu.I[0]);
        }
        /* Header check: does the doomed node ever receive a 0x000218xx allocator
         * header word? (question C from C#). */
        if (pc == STORE884_PC) {
            uint32_t b34 = nd500_read_memory_32(&cpu, cpu.B + 0x34);
            nd500_trap_clear();
            if (b34 == watch && (cpu.I[0] & 0xFFFF0000u) == 0 && (cpu.I[0] & 0xFFFFu) == 0x1802u)
                printf("[HEADER   ] instr=%llu node 0x%08X GETS header 0x%08X via 0x08024884\n",
                       (unsigned long long)cpu.instruction_count, watch, cpu.I[0]);
        }

        /* WRITER: about to store. w1 =: r2.0 -> value in a Wn, target addr in a
         * Wn. Log all four Wn + B when the store targets the root region so we
         * can see the value BEFORE the store runs. */
        if (pc == WRITER_PC) {
            uint32_t w1 = cpu.I[0], w2 = cpu.I[1], w3 = cpu.I[2], w4 = cpu.I[3];
            /* target is whichever Wn is in the root region */
            uint32_t tgt = 0;
            if      ((w2 & ~0xFu) == ROOT_LO) tgt = w2;
            else if ((w1 & ~0xFu) == ROOT_LO) tgt = w1;
            else if ((w3 & ~0xFu) == ROOT_LO) tgt = w3;
            else if ((w4 & ~0xFu) == ROOT_LO) tgt = w4;
            if (tgt >= ROOT_LO && tgt < ROOT_HI) {
                writer_hits++;
                printf("[WRITER #%d] step=%ld instr=%llu PC=0x%08X "
                       "W1=0x%08X W2=0x%08X W3=0x%08X W4=0x%08X B=0x%08X -> target=0x%08X\n",
                       writer_hits, step,
                       (unsigned long long)cpu.instruction_count, pc,
                       w1, w2, w3, w4, cpu.B, tgt);
            }
        }

        /* Free-list membership checkpoints: just before the node is reused, and
         * at the fatal walk. Answers "was the crashing node freed while a list
         * still referenced it?" (use-after-free) vs "reused a live node". */
        if (cpu.instruction_count == 1412860ull) dump_freelist(&cpu, watch, "pre-reuse");
        if (pc == WALK_PC && cpu.I[0] == watch && cpu.instruction_count > 1490000ull)
            dump_freelist(&cpu, watch, "at-fatal-walk");

        /* WALK: `w1 := r1.2` about to load R1 := mem[R1 + off]. Here I[0] is
         * still the SOURCE node. Dump it so we see which offset holds the link
         * being loaded, and whether that link is already garbage. */
        if (pc == WALK_PC) {
            uint32_t node = cpu.I[0];
            uint32_t n0 = nd500_read_memory_32(&cpu, node + 0);
            uint32_t n2 = nd500_read_memory_32(&cpu, node + 2);
            uint32_t n4 = nd500_read_memory_32(&cpu, node + 4);
            uint32_t n8 = nd500_read_memory_32(&cpu, node + 8);
            nd500_trap_clear();
            printf("[WALK     ] step=%ld instr=%llu node(R1)=0x%08X | "
                   "node[+0]=0x%08X node[+2]=0x%08X node[+4]=0x%08X node[+8]=0x%08X\n",
                   step, (unsigned long long)cpu.instruction_count, node,
                   n0, n2, n4, n8);
        }

        /* READER: about to dereference R1 (by test r1.0). Log the pointer AND
         * dump the root region so we can see which offset held the wild value
         * (i.e. what byte offset `w1 := r1.2` actually read from). */
        if (pc == READER_PC) {
            uint32_t w0 = nd500_read_memory_32(&cpu, ROOT_LO + 0);
            uint32_t p2 = nd500_read_memory_32(&cpu, ROOT_LO + 2);
            uint32_t w1_ = nd500_read_memory_32(&cpu, ROOT_LO + 4);
            uint32_t w2_ = nd500_read_memory_32(&cpu, ROOT_LO + 8);
            nd500_trap_clear();  /* our probe reads must not perturb the run */
            printf("[READER   ] step=%ld instr=%llu PC=0x%08X R1=0x%08X | "
                   "root[+0]=0x%08X root[+2]=0x%08X root[+4]=0x%08X root[+8]=0x%08X\n",
                   step, (unsigned long long)cpu.instruction_count, pc, cpu.I[0],
                   w0, p2, w1_, w2_);
        }

        prev_pc = pc;
        nd500_cpu_step(&cpu);

        if (machine.run_flag == 0 && machine.stop_reason != STOP_NONE) {
            printf("[STOP] reason=%s at step=%ld PC=0x%08X instr=%llu\n",
                   nd500_stop_reason_str(machine.stop_reason), step, cpu.PC,
                   (unsigned long long)cpu.instruction_count);
            break;
        }
    }

    printf("Done. steps=%ld writer_root_hits=%d final_PC=0x%08X stop=%s\n",
           step, writer_hits, cpu.PC, nd500_stop_reason_str(machine.stop_reason));
    nd500_machine_free(&machine);
    return 0;
}
