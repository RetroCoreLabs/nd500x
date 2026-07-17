#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * IFKRET instruction - CALL class
 *
 * Mnemonic: ifkret (IF K RET)
 * Operands: 0
 * Opcode: 0x009D
 *
 * Operation: Conditional return if K flag set
 *
 * Description:
 * If the K flag is set, returns from subroutine (same as RET).
 * If the K flag is clear, falls through to next instruction (no return).
 *
 * This instruction combines condition testing with return, allowing compact
 * error handling code. The K flag is cleared during the return (like RET).
 *
 * Operand Structure:
 * - No operands (register-only operation)
 *
 * Operation Steps:
 * 1. Test K flag
 * 2. If K == 0: Fall through to next instruction (no return)
 * 3. If K == 1: Perform RET operation:
 *    a. Clear K flag
 *    b. Read PREVB from B+0, RETA from B+4
 *    c. Check for stack underflow
 *    d. Restore B and jump to return address
 *
 * Flag Behavior:
 * - K: Cleared if return occurs, unchanged if fall-through
 * - Z, S, C, O: Unaffected
 *
 * Trap Conditions:
 * - Stack underflow (STU) if K==1 and PREVB==0
 * - Address trap fetch (ATF) if K==1 and return address invalid
 *
 * Typical Usage:
 *   ; Error handling with conditional return
 *   SUB1:  ENTS #100
 *          CHECK_CONDITION
 *          ; K is set if condition failed
 *          IFKRET        ; Return early if K==1 (error detected)
 *          ; Continue normal processing if K==0
 *          ...
 *          RET           ; Normal return
 *
 * Notes:
 * - Equivalent to: IF K THEN RET
 * - More compact than separate IF K GO + RET sequence
 * - K flag is cleared during return (like RET, not RETK)
 * - Useful for early returns on error conditions
 *
 * Related Instructions:
 * - IF K GO: Conditional branch if K set
 * - RET: Unconditional return (clear K)
 * - RETK: Unconditional return (set K)
 * - IFKRET: Conditional return if K set [this instruction]
 *
 * Reference: ND-500 Reference Manual, Chapter 13
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Ifkret.cs
 */
void nd500_instr_Ifkret(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 0) {
        printf("[ERROR] IFKRET expects 0 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* STEP 1: Test K flag */
    if (!nd500_test_flag(cpu, ND500_FLAG_K)) {
        /* K flag clear - fall through to next instruction (no return) */
        return;
    }

    /* K flag set - perform RET operation */

    /* Stack frame field offsets */
    const uint32_t OFFSET_PREVB = 0;
    const uint32_t OFFSET_RETA  = 4;

    /* STEP 2: the K flag REMAINS SET across the return.
     *
     * ND-05.009.4 EN, "IF K RET" (line 8158): "If the flag bit K is set when the
     * IF K RET instruction is executed, a subroutine return is performed WITH THE
     * FLAG BIT REMAINING SET." This is the whole point of the instruction - it is
     * how an error (K set by a MON call or a failed operation) propagates up a
     * chain of "MON ...; ifkret" wrapper routines to the caller that finally
     * checks it.
     *
     * This code previously CLEARED K here ("RET behaviour, not RETK"), which
     * silently swallowed the error after ONE level. The ND linker's OPEN-DOMAIN
     * reads block 0 of a new domain through four nested ifkret wrappers; with K
     * cleared, the top caller saw K-clear = "read succeeded", used a garbage
     * buffer as the domain header, and aborted with error 41B instead of taking
     * its new-domain path. Do NOT clear K here. */
    uint32_t prev_b = nd500_read_memory_32(cpu, cpu->B + OFFSET_PREVB);
    uint32_t ret_addr = nd500_read_memory_32(cpu, cpu->B + OFFSET_RETA);

    /* STEP 4: Check for domain boundary */
    if ((prev_b == 0 || ret_addr == 0) && (cpu->CAD != cpu->CED) && (cpu->CAD != 0)) {
        printf("[TODO] IFKRET at PC=0x%08X: Domain return not yet implemented\n", fi->address);
        trap_stack_underflow(cpu, fi->address);
        return;
    }

    /* STEP 5: Check for stack underflow */
    if (prev_b == 0) {
        printf("[TRAP] IFKRET at PC=0x%08X: Stack underflow (PREVB=0)\n", fi->address);
        trap_stack_underflow(cpu, fi->address);
        return;
    }

    /* STEP 6: Normal return - restore stack frame */
    cpu->PC = ret_addr;
    cpu->L = ret_addr;
    cpu->B = prev_b;
}
