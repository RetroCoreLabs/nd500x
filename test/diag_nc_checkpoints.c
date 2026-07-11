/*
 * Diagnostic: NC codegen full-run checkpoint trace for the cross-emulator diff.
 *
 * Drives the real NC compiler (nc-a06.dom) with "COMPILE A,A,A" under the PINNED
 * deterministic clock and emits a checkpoint line every CHECKPOINT_STRIDE (5000)
 * instructions, starting at instruction 0 (PC 0x08000004). Format locked with the
 * RetroCore (C#) side:
 *
 *     instr#  PC  B  I1 I2 I3 I4      (all hex, no 0x prefix, space-separated)
 *
 * The C# side emits the identical format; a line-by-line diff by instruction
 * number brackets the FIRST divergent instruction to one 5000-instruction window.
 *
 * IMPORTANT: run with the pinned clock so the stream matches the agreed instant
 * (1990-01-01 12:00:00 UTC): set ND500X_PIN_CLOCK=1 in the environment. Run from
 * build/nc_sandbox so the SINTRAN source files (A:C etc.) resolve.
 *
 * Build (same static-lib link line as diag_nc_writer_watch):
 *   gcc -O2 -o build/bin/diag_nc_checkpoints test/diag_nc_checkpoints.c \
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
#include "../src/libmon/mon.h"
#include "../src/libmon/mon_file_table.h"
#include "../src/libmon/mon_clock.h"

#define MEMORY_SIZE        (16u * 1024u * 1024u)
#define CHECKPOINT_STRIDE  5000ull

int main(int argc, char** argv) {
    const char* dom = (argc > 1) ? argv[1] : "/mnt/d/ND/500/FraTor/nc/nc-a06.dom";
    const char* cmd = (argc > 2) ? argv[2] : "COMPILE A,A,A\r";
    long max_steps  = (argc > 3) ? strtol(argv[3], NULL, 0) : 2000000L;
    const char* out = (argc > 4) ? argv[4] : NULL;

    FILE* fp = stdout;
    if (out) {
        fp = fopen(out, "w");
        if (!fp) { fprintf(stderr, "cannot open %s\n", out); return 2; }
    }

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
    mon_init();   /* reads ND500X_PIN_CLOCK for the deterministic clock */

    /* Queue the compile command(s). Both "\r" and the ";;" separator become CR,
     * matching diag_nc_pctrace so the two tools drive NC identically. */
    char inbuf[128];
    size_t j = 0;
    for (size_t i = 0; cmd[i] && j < sizeof(inbuf) - 2; i++) {
        if (cmd[i] == '\\' && cmd[i + 1] == 'r') { inbuf[j++] = '\r'; i++; }
        else if (cmd[i] == ';' && cmd[i + 1] == ';') { inbuf[j++] = '\r'; i++; }
        else inbuf[j++] = cmd[i];
    }
    inbuf[j] = 0;
    mon_queue_console_input(inbuf);

    uint32_t start_addr = 0; int domain = 0;
    if (ndlib_dom_load_to_machine(&machine, &cpu, -1, NULL, NULL,
                                  &start_addr, &domain) != 0) {
        fprintf(stderr, "dom load-to-machine failed\n"); return 2;
    }

    fprintf(stderr, "Checkpoint trace: start PC=0x%08X domain=%d stride=%llu clock=%s\n",
            cpu.PC, domain, (unsigned long long)CHECKPOINT_STRIDE,
            mon_clock_is_deterministic() ? "PINNED" : "REAL(non-deterministic!)");

    /* Header comment so the file is self-describing; the diff tool skips '#'. */
    fprintf(fp, "# instr PC B I1 I2 I3 I4  (hex, stride %llu, pinned=%s)\n",
            (unsigned long long)CHECKPOINT_STRIDE,
            mon_clock_is_deterministic() ? "yes" : "NO");

    machine.run_flag = 1;
    machine.stop_reason = STOP_NONE;

    unsigned long long next_cp = 0;   /* next instruction number to record */
    long step = 0;
    long emitted = 0;
    for (; step < max_steps && machine.run_flag; step++) {
        /* At the top of the loop cpu.instruction_count is the index of the
         * instruction ABOUT to execute, and cpu.PC is its address. Emit a
         * checkpoint whenever we reach (or pass) the next stride boundary. */
        if (cpu.instruction_count >= next_cp) {
            fprintf(fp, "%llu %08X %08X %08X %08X %08X %08X\n",
                    (unsigned long long)cpu.instruction_count,
                    cpu.PC, cpu.B,
                    cpu.I[0], cpu.I[1], cpu.I[2], cpu.I[3]);
            emitted++;
            next_cp = cpu.instruction_count - (cpu.instruction_count % CHECKPOINT_STRIDE)
                    + CHECKPOINT_STRIDE;
        }

        nd500_cpu_step(&cpu);

        if (machine.run_flag == 0 && machine.stop_reason != STOP_NONE) {
            break;
        }
    }

    /* Final line: the exact stopping point (crash), regardless of stride. */
    fprintf(fp, "%llu %08X %08X %08X %08X %08X %08X  # STOP=%s\n",
            (unsigned long long)cpu.instruction_count,
            cpu.PC, cpu.B, cpu.I[0], cpu.I[1], cpu.I[2], cpu.I[3],
            nd500_stop_reason_str(machine.stop_reason));

    fprintf(stderr, "Done. steps=%ld checkpoints=%ld final_PC=0x%08X stop=%s\n",
            step, emitted, cpu.PC, nd500_stop_reason_str(machine.stop_reason));

    if (fp != stdout) fclose(fp);
    nd500_machine_free(&machine);
    return 0;
}
