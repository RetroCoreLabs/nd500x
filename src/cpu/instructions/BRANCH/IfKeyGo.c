#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * IfKeyGo instruction - BRANCH class
 *
 * Conditional jump if K flag (Key/Invalid/Destination Full) is NOT set.
 *
 * Variants: 2 (by displacement size)
 * Mnemonics: IF -K GO:B, IF -K GO:H
 * Operands: 1 (signed displacement)
 *
 * Opcodes:
 *   0x00D2 (IF -K GO:B) - Byte displacement
 *   0x00D3 (IF -K GO:H) - Halfword displacement
 *
 * Operation: if K = 0 then PC <- PC + displacement
 *
 * Description:
 *   A conditional jump causes transfer of control if and only if the K
 *   (Key/Invalid/Destination Full) flag is CLEAR (K=0). The sign-extended byte
 *   or halfword displacement is added to the program counter.
 *
 *   The K flag has multiple uses in the ND-500 architecture:
 *   - Invalid Operation: Set when an instruction encounters invalid data or state
 *   - Destination Full: Set by queue/stack operations when destination is full
 *   - Key Match: Set by some search/comparison operations
 *   - BCD Invalid: Set by packed decimal operations on malformed BCD data
 *
 *   This instruction branches when K is NOT set, typically to skip error handling.
 *
 * Displacement Encoding:
 *   - BY variant (0x00D2): 8-bit signed displacement (-128 to +127 bytes)
 *   - H variant (0x00D3): 16-bit signed displacement (-32768 to +32767 bytes)
 *   - Displacement is sign-extended to 32 bits before adding to PC
 *   - Branch target: PC_current + displacement
 *
 * Operand Structure:
 *   - Operand[0]: Displacement value (read, byte or halfword based on variant)
 *     * Effective address contains the displacement
 *     * Sign-extended based on data type (BY -> int8, H -> int16)
 *
 * Operation Steps:
 *   1. Read K flag from CPU status register
 *   2. If K flag is clear (K = 0):
 *      a. Read displacement from operand[0]
 *      b. Sign-extend displacement based on data type
 *      c. Add displacement to PC
 *   3. If K flag is set (K = 1):
 *      a. Fall through to next instruction (no branch)
 *
 * Flag Behavior:
 *   - All flags unaffected (Z, S, C, K, O remain unchanged)
 *
 * Branch Examples:
 *
 *   Example 1: Forward branch if K clear
 *     Address 0x1000: IF -K GO:B #20   ; Jump forward 20 bytes if K=0
 *     K=0 -> PC becomes 0x1000 + 20 = 0x1014
 *     K=1 -> PC advances normally to next instruction
 *
 *   Example 2: Backward branch if K clear
 *     Address 0x2000: IF -K GO:B #-50  ; Jump backward 50 bytes if K=0
 *     K=0 -> PC becomes 0x2000 + (-50) = 0x1FCE
 *     K=1 -> PC advances normally to next instruction
 *
 *   Example 3: Large displacement with halfword variant
 *     Address 0x3000: IF -K GO:H #1000 ; Jump forward 1000 bytes if K=0
 *     K=0 -> PC becomes 0x3000 + 1000 = 0x33E8
 *     K=1 -> PC advances normally to next instruction
 *
 * Trap Conditions:
 *   - Addressing traps if displacement calculation results in invalid address
 *   - Branch trap (BT) may be raised depending on system configuration
 *   - Illegal Operand if operand count is not exactly 1
 *
 * Typical Usage:
 *   ; Skip error handler after successful MON call (K=0 on success)
 *   MON GSWSP                ; Get scratch workspace (K=0 if success)
 *   IF -K GO:B CONTINUE      ; Branch past error handler if K clear
 *   ; error handling code
 *   CONTINUE:
 *
 *   ; Skip error path on successful operation
 *   CALL OPERATION           ; Operation that sets K=0 on success
 *   IF -K GO:H SUCCESS       ; Branch to success path if K clear
 *
 *   ; Conditional execution when flag is NOT set
 *   TST CONDITION            ; Test condition (may clear K)
 *   IF -K GO:B CLEARED       ; Branch if K was cleared
 *
 * Notes:
 *   - This is a conditional relative branch (PC-relative addressing)
 *   - Displacement is relative to current PC, not next instruction
 *   - Unlike absolute jumps, this preserves position-independent code
 *   - Branch range: +/-127 bytes (BY) or +/-32767 bytes (H)
 *   - K flag is NOT cleared by this instruction (remains set for error handling)
 *   - Use CLK instruction to explicitly clear K flag when needed
 *   - Useful for error handling, queue overflow detection, search results
 *
 * Comparison with Other Instructions:
 *   - IF -K GO: Branch if K flag clear (this instruction, opcodes 0xD2/0xD3)
 *   - IF K GO: Branch if K flag set (opposite condition, opcodes 0xD0/0xD1)
 *   - IF = GO: Branch if Z flag set (zero/equal)
 *   - GO: Unconditional branch (always jumps)
 *
 * Performance:
 *   - Branch taken: ~5-8 CPU cycles (depends on displacement size)
 *   - Branch not taken: ~3-4 CPU cycles (fall through)
 *   - No pipeline flush on ND-500 (in-order execution)
 *
 * Reference: ND-500 Reference Manual, Chapter 13.3 (Conditional Branches)
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/BRANCH/IfKeyGo.cs
 */
void nd500_instr_IfKeyGo(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] IF K GO at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check K flag condition - IF -K GO branches when K is NOT set */
    if (!nd500_test_flag(cpu, ND500_FLAG_K)) {
        /* Read displacement value */
        uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }

        /* Sign-extend based on data type */
        int64_t displacement = nd500_sign_extend_by_dtype(value, fi->data_type);

        /* Update PC (relative branch from instruction start) */
        cpu->PC = (uint32_t)(fi->address + displacement);
    }
    /* else: K flag SET, fall through to next instruction (no branch) */

    /* Flags unaffected */
}
