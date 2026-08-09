#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Tutti instruction - SYSTEM class
 *
 * TUTTI - Enable Process Switch
 *
 * Format: TUTTI
 *
 * Assembly:
 *   TUTTI (enable process switch)               Hex 0xFE01
 *
 * Operation: process switch is enabled
 *
 * Description:
 *   Enables process switching. Used with SOLO to implement critical sections.
 *   Ignorable trap conditions are ignored in SOLO-TUTTI sequences regardless
 *   of enabling of these traps.
 *
 * Trap conditions: None (manual ch.16.2)
 *
 * Data status bits: None affected
 *
 * NOT PRIVILEGED - corrected 2026-08-09. This routine used to open with
 * nd500_require_privilege() and its comment called TUTTI "Privileged", which
 * came from the C# port rather than from any source. Three things say
 * otherwise:
 *
 *   1. The manual marks privileged instructions with an explicit
 *      "Privileged instruction" line in the Description - see 15.17 CLINIT and
 *      16.13 DMON, both of which still say "Trap Conditions: None", so the
 *      trap list is NOT where privilege is recorded. Neither 16.1 SOLO nor
 *      16.2 TUTTI carries that line.
 *   2. Ch.16.1 presumes unprivileged users execute SOLO: "Unprivileged users
 *      are not allowed to run in SOLO for more than 256 cycles." A privileged
 *      -only SOLO would make that rule, and the whole DT timeout, dead code.
 *   3. The ND-5000 control store has no privilege test in TUTTI_0 (004534-
 *      004537). SOLO_0 does read PIA, but only to decide whether to arm the
 *      timeout (004530/004531), never to reject the instruction.
 *
 * The bug was worse than a stray check: SOLO here has NO privilege guard, so
 * unprivileged code could open a SOLO region and then be refused the only
 * instruction that closes it. It was found by a test asserting that a region
 * closed by TUTTI cannot later raise DT - the TUTTI trapped instead, PSD
 * stayed set, and DT fired long afterwards.
 *
 * Reference: ND-500 Reference Manual, Chapter 16.2
 */
void nd500_instr_Tutti(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {

    /* Validate operand count (like C# lines 40-45) */
    if (fi->operand_count != 0) {
        printf("[ERROR] TUTTI at PC=0x%08X: Expected 0 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Clear Process Switch Disabled flag.
     * Reference: manual ch.16.2 - "allows normal interleaving of process
     * execution". Clearing PSD ends the SOLO region, which also stands down
     * the DT timeout measurement (it only runs while PSD is set) and the DE
     * check on the non-ignorable trap path. */
    cpu->ST1 &= ~ND500_FLAG_PSD;
    cpu->solo_start_icount = 0;

    /* No status bits affected for this instruction */
}
