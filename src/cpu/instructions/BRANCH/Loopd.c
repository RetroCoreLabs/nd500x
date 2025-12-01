#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Loopd instruction - BRANCH class
 *
 * LOOPD - Loop with Decrement (Decrement index and conditionally branch)
 *
 * Mnemonic: LOOPD
 * Format: t LOOPD <index/rw/t>, <limit/r/t>, <<displacement>>
 * Variants: 10 (BY, H, W, F, D with B/H displacement)
 * Operands: 3 (<index/rw>, <limit/r>, <<displacement>>)
 *
 * Opcodes:
 *   0xFD23 (BY LOOPD:B) - Byte index, byte displacement
 *   0xFD28 (BY LOOPD:H) - Byte index, halfword displacement
 *   0xFD24 (H LOOPD:B)  - Halfword index, byte displacement
 *   0xFD29 (H LOOPD:H)  - Halfword index, halfword displacement
 *   0xFD25 (W LOOPD:B)  - Word index, byte displacement
 *   0xFD2A (W LOOPD:H)  - Word index, halfword displacement
 *   0xFD26 (F LOOPD:B)  - Float index, byte displacement
 *   0xFD2B (F LOOPD:H)  - Float index, halfword displacement
 *   0xFD27 (D LOOPD:B)  - Double index, byte displacement
 *   0xFD2C (D LOOPD:H)  - Double index, halfword displacement
 *
 * Operation:
 *   <index> - 1 -> <index>
 *   if (<index> - <limit>) < 0 then
 *       address of next instruction -> PC (fall through)
 *   else
 *       PC + <<displacement>> -> PC (loop back)
 *   endif
 *
 * Description:
 *   Implements a loop with decrementing index counter. The instruction first
 *   decrements the index operand by one, writes it back, then compares the
 *   modified index with the limit value.
 *
 *   If the decremented index is greater than or equal to the limit (signed
 *   comparison), the loop continues by adding the signed displacement to PC,
 *   typically jumping backwards to the start of the loop body.
 *
 *   If the decremented index is less than the limit, the loop terminates and
 *   execution falls through to the next instruction.
 *
 *   This instruction is typically placed at the end of a loop body with a
 *   negative displacement value to jump back to the beginning. The loop runs
 *   while index >= limit, enabling countdown loops.
 *
 * Comparison Logic:
 *   Uses signed comparison for all integer types (BY, H, W)
 *   Float/Double variants use floating-point comparison (not yet implemented)
 *
 * Displacement Encoding:
 *   - :B variant: Signed 8-bit displacement (-128 to +127 bytes)
 *   - :H variant: Signed 16-bit displacement (-32768 to +32767 bytes)
 *   - Typically negative to jump backwards to loop start
 *
 * Flags: Z (zero), S (sign) - based on modified index value
 *   Z = 1 if modified index equals 0
 *   Z = 0 if modified index is non-zero
 *   S = 1 if modified index is negative (sign bit set)
 *   S = 0 if modified index is non-negative
 *
 *   Note: Flags reflect the decremented index value, not the comparison result
 *
 * Trap conditions:
 *   - Addressing traps for operand access
 *   - BT (Branch Trap) if target address protection violation
 *   - Page fault if target page not present
 *
 * Performance:
 *   - Typical: 3-4 cycles
 *   - Loop taken (branch): 3-4 cycles
 *   - Loop exit (fall through): 2-3 cycles
 *
 * Key Characteristics:
 *   - Atomic decrement-compare-branch operation
 *   - Signed comparison (index >= limit continues loop)
 *   - Decrements before comparison
 *   - Z/S flags reflect decremented index
 *   - Supports byte and halfword displacement
 *   - PC-relative branching (not absolute)
 *   - Ideal for countdown loops
 *   - Index is read-write operand (modified in place)
 *
 * Common Use Cases:
 *   - Countdown loops (for i = N down to 0)
 *   - Array traversal from high to low index
 *   - Stack unwinding loops
 *   - Reverse iteration over data structures
 *   - Loop counters that decrement to zero
 *   - Backward string processing
 *
 * Example Usage:
 *   ; Countdown loop from 10 to 1
 *   I1 = 10                    ; Initialize index
 *   LOOP_START:
 *       ; Loop body code here
 *       PROCESS I1             ; Use index value
 *       W LOOPD:B I1, 1, LOOP_START  ; Decrement and loop if >= 1
 *   ; Falls through when I1 reaches 0
 *
 *   ; Array traversal from end to start
 *   I2 = ARRAY_SIZE - 1        ; Start at last element
 *   TRAVERSE_LOOP:
 *       I3 = ARRAY(I2)         ; Load element
 *       PROCESS I3             ; Process element
 *       W LOOPD:B I2, 0, TRAVERSE_LOOP  ; Decrement, loop while >= 0
 *
 *   ; Countdown with memory operand
 *   COUNTER = 100              ; Set counter in memory
 *   WORK_LOOP:
 *       ; Perform work
 *       DO_WORK
 *       W LOOPD:H COUNTER, 0, WORK_LOOP  ; Loop 100 times
 *
 *   ; Reverse string copy
 *   I1 = STRING_LEN - 1        ; Start at last character
 *   COPY_LOOP:
 *       I2 = SRC_STRING(I1)
 *       DEST_STRING(I1) = I2
 *       BY LOOPD:B I1, 0, COPY_LOOP  ; Decrement byte index
 *
 * Related Instructions:
 *   - LOOPI: Loop with increment (counts up)
 *   - LOOP: General loop with configurable step
 *   - IF conditions: Simple conditional branches
 *   - GO: Unconditional relative branch
 *
 * Comparison with Related Instructions:
 *   - LOOPD vs LOOPI: Decrements vs increments, >= vs <= comparison
 *   - LOOPD vs LOOP: Fixed -1 step vs configurable step
 *   - LOOPD vs IF<GO: Combined decrement+compare vs separate operations
 *
 * Typical Pattern (Countdown Loop):
 *   ; Initialize counter
 *   I1 = N
 *
 *   LOOP_BODY:
 *       ; Use counter value
 *       PROCESS I1
 *
 *       ; Decrement and loop
 *       W LOOPD:B I1, 1, LOOP_BODY
 *
 *   ; Loop exits when I1 < 1 (i.e., I1 reaches 0)
 *
 * Loop Termination:
 *   The loop terminates when (index - 1) < limit
 *   For countdown to zero: use limit = 0 or 1 depending on whether you want
 *   to execute loop body when index = 0
 *
 * Signed vs Unsigned:
 *   LOOPD uses signed comparison, making it unsuitable for unsigned countdown
 *   loops that cross zero. Use explicit comparison and DECR for unsigned loops.
 *
 * Float/Double Support:
 *   Float and Double variants exist but are not commonly used. They perform
 *   floating-point decrement by 1.0 and floating-point comparison.
 */
void nd500_instr_Loopd(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count
    if (fi->operand_count != 3) {
        printf("[ERROR] LOOPD at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Read index and limit
    uint64_t index = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    uint64_t limit = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

    // Decrement index by 1
    index--;

    // Write decremented index back
    nd500_write_operand_value(cpu, &fi->operands[0], index, fi->data_type);

    // Perform signed comparison based on data type
    bool should_loop = false;

    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            should_loop = ((int8_t)index >= (int8_t)limit);
            break;
        case ND500_DTYPE_HALFWORD:
            should_loop = ((int16_t)index >= (int16_t)limit);
            break;
        case ND500_DTYPE_WORD:
            should_loop = ((int32_t)index >= (int32_t)limit);
            break;
        case ND500_DTYPE_DOUBLEWORD:
            // Float/Double not yet implemented
            printf("[STUB] LOOPD at PC=0x%08X: Float/Double not implemented\n", fi->address);
            return;
        default:
            printf("[ERROR] LOOPD at PC=0x%08X: Invalid data type %u\n",
                   fi->address, fi->data_type);
            trap_invalid_operation(cpu, fi->address);
            return;
    }

    if (should_loop) {
        // Jump back to start of loop (PC + displacement -> PC)
        uint64_t value = nd500_read_operand_value(cpu, &fi->operands[2], fi->data_type);
        int64_t displacement = nd500_sign_extend_by_dtype(value, fi->data_type);
        cpu->PC = (uint32_t)(fi->address + fi->total_len + displacement);
    }
    // else: fall through to next instruction

    // Update Z and S flags based on decremented index
    nd500_set_flags_zs(cpu, index, fi->data_type);
}
