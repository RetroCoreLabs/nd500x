/*
 * Ifkgo.c - ND-500 Ifkgo instruction (BRANCH class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Ifkgo instruction - BRANCH class
 *
 * IFKGO - If K Flag Set, Go (Conditional Branch on User Flag)
 *
 * Mnemonic: IFKGO
 * Format: IFKGO <displacement>
 * Variants: 2
 * Operands: 1 (displacement)
 *
 * Opcodes:
 *   0x00D0 (IFKGO:B) - Branch with byte displacement (-128 to +127)
 *   0x00D1 (IFKGO:H) - Branch with halfword displacement (-32768 to +32767)
 *
 * Operation:
 *   if (K flag == 1) then
 *       PC = PC + sign_extend(displacement)
 *   else
 *       PC = next instruction
 *   endif
 *
 * Description:
 *   Performs a conditional branch if the K (user flag) bit in the status
 *   register is set (K=1). If the condition is true, the sign-extended
 *   displacement is added to the program counter, transferring control to
 *   the target address. If K=0, execution continues with the next instruction.
 *
 *   This instruction is part of the ND-500's comprehensive conditional branch
 *   system. Unlike other conditional branches that test arithmetic flags
 *   (Z, S, C, V), IFKGO tests the user-controlled K flag, which is set/cleared
 *   explicitly by SETK/CLRK instructions. This enables custom control flow
 *   patterns based on application-specific conditions.
 *
 *   The K flag (bit 8 of ST1) is a user-programmable flag independent of
 *   arithmetic operations. It's commonly used for:
 *     - Boolean variables and custom condition flags
 *     - Semaphore/lock status checking
 *     - State machine transitions
 *     - Error flag testing
 *     - Feature enable/disable branching
 *     - Event signaling in interrupt handlers
 *
 * Displacement Encoding:
 *   - Byte displacement (0x00D0): Signed 8-bit (-128 to +127 bytes)
 *     Used for short forward/backward jumps within +/-127 bytes
 *   - Halfword displacement (0x00D1): Signed 16-bit (-32768 to +32767 bytes)
 *     Used for longer jumps within +/-32KB
 *   - Assembler auto-selects optimal displacement size
 *
 * Branch Target Calculation:
 *   target_address = address_of_IFKGO + sign_extend(displacement)
 *   The displacement is relative to the first byte of the IFKGO
 *   instruction itself (manual section 8.16.1), verified against a
 *   real SINTRAN-linked binary.
 *
 * Flags: None modified
 *   K, Z, S, C, V - All flags remain unchanged
 *   Branch does not affect status flags
 *
 * Trap conditions:
 *   - Addressing traps if target address is invalid
 *   - Branch trap (BT) if target protection violation
 *   - Page fault if target page not present
 *
 * Performance:
 *   - Branch taken: 1-2 cycles
 *   - Branch not taken: 1 cycle
 *   - No pipeline flush on modern implementations
 *
 * Key Characteristics:
 *   - Tests user-controlled K flag (not arithmetic flags)
 *   - Two displacement sizes: byte (+/-127), halfword (+/-32767)
 *   - Paired with SETK/CLRK for custom control flow
 *   - Independent of Z, S, C, V flags (orthogonal branching)
 *   - Essential for semaphores, state machines, error flags
 *   - Fast conditional branch (1-2 cycles)
 *   - Common in event handling and feature toggles
 *
 * Common Use Cases:
 *   - Testing custom condition flags set by SETK
 *   - Semaphore/lock checking before critical section
 *   - State machine transitions based on mode flags
 *   - Error flag testing after operations
 *   - Feature enable/disable branching
 *   - Event signaling in interrupt handlers
 *   - Boolean variable testing
 *
 * Example Usage:
 *   ; Check if error occurred
 *   CALL OPERATION       ; May set K flag on error
 *   IFKGO:B ERROR_HANDLER
 *   ; No error, continue normal flow
 *   ...
 *   ERROR_HANDLER:
 *   ; Handle error condition
 *
 *   ; Semaphore check
 *   SETK                 ; Assume locked
 *   TEST_AND_SET LOCK
 *   IF=0 Z CLRK          ; Clear K if lock acquired
 *   IFKGO:B WAIT_LOOP    ; Branch if still locked (K=1)
 *   ; Lock acquired, enter critical section
 *
 * Related Instructions:
 *   - SETK: Set K flag to 1
 *   - CLRK: Clear K flag to 0
 *   - IFNKGO: Branch if K flag is clear (K=0)
 *   - Other conditional branches: IF=GO, IF><GO, IF<GO, IF>GO, IF<=GO, IF>=GO
 *
 * Reference: ND-500 Reference Manual, section 13.x (Conditional Branches)
 *            docs/instructions/asm/ifkgo.md
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/BRANCH/Ifkgo.cs
 */
void nd500_instr_Ifkgo(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count (should have 1 displacement operand)
    if (fi->operand_count != 1) {
        printf("[ERROR] IFKGO at PC=0x%08X: Expected 1 operand (displacement), got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Read the displacement operand (signed byte or halfword)
    // The displacement is a direct operand (part of instruction encoding)
    int32_t displacement = (int32_t)nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    // Sign-extend displacement based on data type
    if (fi->data_type == ND500_DTYPE_BYTE) {
        // Byte displacement: sign-extend 8-bit to 32-bit
        displacement = (int8_t)(displacement & 0xFF);
    } else if (fi->data_type == ND500_DTYPE_HALFWORD) {
        // Halfword displacement: sign-extend 16-bit to 32-bit
        displacement = (int16_t)(displacement & 0xFFFF);
    }

    // Test K flag (bit 8 of ST1)
    bool k_flag_set = (cpu->ST1 & ND500_FLAG_K) != 0;

    // Conditional branch based on K flag
    if (k_flag_set) {
        // K=1: Take the branch
        // Displacement is relative to the FIRST byte of this instruction
        // (manual 8.16.1), same as all other GO/IF..GO/LOOP instructions.
        cpu->PC = (uint32_t)(fi->address + displacement);

        // Note: No flags are modified by IFKGO
        // The K flag remains set, and all other flags are unchanged
    }
    // else: K=0, branch not taken, PC already points to next instruction

    // No status flags are modified by this instruction
}
