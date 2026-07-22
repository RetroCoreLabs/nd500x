/*
 * Diagnostic: NC TERMINAL codegen-loop operand probe (Phase 1, UPDATE 63g).
 *
 * NON-PERTURBING: reads virtual memory via nd500_mmu_peek (trap-free translate),
 * NOT nd500_mmu_translate (which raises page-fault/protect traps as a side effect
 * and corrupts the run - see UPDATE 63e).
 *
 * The terminal runaway (onset ~instr 1-2M) is an exact 92-instr loop, B=0x10000174,
 * with the decisive branch 0x0802BA89 `w comp2 r1.27,W2 ; if<<go` taken every
 * iteration. This probe stops at 0x0802BA89 (after instr 2,000,000 to be in the
 * terminal loop), dumps the two comp2 operands (op1 = word[W1+27], op2 = W2) and the
 * seg-3 record W1 points at, for several iterations. If op1/op2 never change, the
 * loop bound is wrong (unbounded emit) or the count store does not persist.
 *
 * Run PINNED from build/nc_sandbox:  ND500X_PIN_CLOCK=1 ../bin/diag_codegen_loop
 * Build: gcc -O2 -o build/bin/diag_codegen_loop test/diag_codegen_loop.c \
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
#include <ndmon/mon_file_table.h>
#include <ndmon/mon_clock.h>

#define MEMORY_SIZE   (16u * 1024u * 1024u)
#define CMP_PC        0x0802BA89u    /* w comp2 r1.27,W2 (terminal-loop exit test) */
#define START_INSTR   2000000ull     /* only sample once in the terminal loop */
#define MAX_STEPS     4000000L
#define MAX_HITS      8

int main(int argc, char** argv) {
    const char* dom = (argc > 1) ? argv[1] : "/mnt/d/ND/500/FraTor/nc/nc-a06.dom";
    const char* cmd = "COMPILE A,A,A\r";

    if (ndlib_load_dom_header(dom) != 0) { fprintf(stderr, "header load failed\n"); return 2; }
    if (ndlib_load_dom_segments() != 0)  { fprintf(stderr, "segment load failed\n"); return 2; }

    Nd500Machine machine;
    nd500_machine_init(&machine, MEMORY_SIZE);
    if (!machine.memory) { fprintf(stderr, "machine init failed\n"); return 2; }

    Nd500Cpu cpu;
    nd500_cpu_init(&cpu, &machine);
    nd500_cpu_reset(&cpu);
    nd500_mmu_init(&cpu);
    nd500_domain_init(&cpu);
    mon_init();

    char inbuf[64]; size_t j = 0;
    for (size_t i = 0; cmd[i] && j < sizeof(inbuf) - 2; i++) {
        if (cmd[i] == '\\' && cmd[i+1] == 'r') { inbuf[j++] = '\r'; i++; }
        else inbuf[j++] = cmd[i];
    }
    inbuf[j] = 0;
    mon_queue_console_input(inbuf);

    uint32_t start_addr = 0; int domain = 0;
    if (ndlib_dom_load_to_machine(&machine, &cpu, -1, NULL, NULL,
                                  &start_addr, &domain) != 0) {
        fprintf(stderr, "dom load-to-machine failed\n"); return 2;
    }

    machine.run_flag = 1;
    machine.stop_reason = STOP_NONE;

    /* trap-free virtual word read: returns 0xDEADxxxx sentinel if unmapped */
    #define PEEK32(va) ({ uint32_t _p = nd500_mmu_peek(&cpu, (va)); \
        (_p == 0xFFFFFFFFu) ? 0xDEAD0000u : nd500_bus_read32(&machine, _p); })

    long step = 0; long long hits = 0;
    uint32_t prev_op1 = 0xFFFFFFFFu;
    for (; step < 40000000L && machine.run_flag; step++) {
        if (cpu.PC == CMP_PC && cpu.instruction_count >= START_INSTR) {
            uint32_t r1 = cpu.I[0];
            uint32_t w2 = cpu.I[1];
            uint32_t op1 = PEEK32(r1 + 27);
            /* Print the first few, then only when op1 DECREASES (reset/wrap) or W2 changes,
             * plus a sparse heartbeat, to reveal the counter trajectory over 38M instrs. */
            if (hits < 4 || op1 < prev_op1 || (hits % 20000 == 0)) {
                printf("HIT %lld instr=%llu  r1=%08X W2=%08X op1=[r1+27]=%08X  op1<W2=%d %s\n",
                       hits, (unsigned long long)cpu.instruction_count, r1, w2, op1,
                       (op1 < w2), (op1 < prev_op1 && prev_op1 != 0xFFFFFFFFu) ? "<== RESET/DROP" : "");
            }
            prev_op1 = op1;
            hits++;
        }
        nd500_cpu_step(&cpu);
        if (machine.run_flag == 0 && machine.stop_reason != STOP_NONE) break;
    }
    (void)MAX_STEPS; (void)MAX_HITS;
    #undef PEEK32
    fprintf(stderr, "Done. steps=%ld hits=%d final_PC=0x%08X stop=%s\n",
            step, hits, cpu.PC, nd500_stop_reason_str(machine.stop_reason));
    nd500_machine_free(&machine);
    return 0;
}
