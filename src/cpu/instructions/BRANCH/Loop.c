#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Loop instruction - BRANCH class
 *
 * LOOP - Loop General (Add step to index and conditionally branch)
 *
 * Mnemonic: LOOP
 * Format: t LOOP <index/rw/t>, <step/r/t>, <limit/r/t>, <<displacement>>
 * Variants: 10 (BY, H, W, F, D with B/H displacement)
 * Operands: 4 (<index/rw>, <step/r>, <limit/r>, <<displacement>>)
 *
 * Opcodes:
 *   0xFD2D (BY LOOP:B) - Byte index/step/limit, byte displacement
 *   0xFD32 (BY LOOP:H) - Byte index/step/limit, halfword displacement
 *   0xFD2E (H LOOP:B)  - Halfword index/step/limit, byte displacement
 *   0xFD33 (H LOOP:H)  - Halfword index/step/limit, halfword displacement
 *   0xFD2F (W LOOP:B)  - Word index/step/limit, byte displacement
 *   0xFD34 (W LOOP:H)  - Word index/step/limit, halfword displacement
 *   0xFD30 (F LOOP:B)  - Float index/step/limit, byte displacement
 *   0xFD35 (F LOOP:H)  - Float index/step/limit, halfword displacement
 *   0xFD31 (D LOOP:B)  - Double index/step/limit, byte displacement
 *   0xFD36 (D LOOP:H)  - Double index/step/limit, halfword displacement
 *
 * Operation:
 *   <index> + <step> -> <index>
 *   if <index> <= <limit> then
 *       PC + <<displacement>> -> PC (loop back)
 *   else
 *       address of next instruction -> PC (fall through)
 *   endif
 *
 * Description:
 *   Implements a general-purpose loop with configurable step size. The
 *   instruction adds the step value to the index, writes it back, then
 *   compares the modified index with the limit value.
 *
 *   If the updated index is less than or equal to the limit (signed
 *   comparison), the loop continues by adding the signed displacement to PC,
 *   typically jumping backwards to the start of the loop body.
 *
 *   If the updated index is greater than the limit, the loop terminates and
 *   execution falls through to the next instruction.
 *
 *   The step operand enables flexible iteration: positive values for count-up
 *   loops, negative values for countdown loops, and arbitrary step sizes for
 *   strided iteration (e.g., processing every Nth element).
 *
 *   This instruction is typically placed at the end of a loop body with a
 *   negative displacement value to jump back to the beginning.
 *
 * Comparison Logic:
 *   Uses signed comparison for all integer types (BY, H, W)
 *   Float/Double variants use floating-point comparison (not yet implemented)
 *
 * Step Value:
 *   - Positive: Count up (like LOOPI with custom increment)
 *   - Negative: Count down (like LOOPD with custom decrement)
 *   - Zero: Infinite loop (not recommended)
 *   - Any magnitude: Strided loops (e.g., step=2 for even indices)
 *
 * Displacement Encoding:
 *   - :B variant: Signed 8-bit displacement (-128 to +127 bytes)
 *   - :H variant: Signed 16-bit displacement (-32768 to +32767 bytes)
 *   - Typically negative to jump backwards to loop start
 *
 * Flags: None modified
 *   Unlike LOOPI and LOOPD, the general LOOP instruction does not modify
 *   any status flags. The index value is updated but flags remain unchanged.
 *
 * Trap conditions:
 *   - Addressing traps for operand access
 *   - BT (Branch Trap) if target address protection violation
 *   - Page fault if target page not present
 *
 * Performance:
 *   - Typical: 4-5 cycles
 *   - Loop taken (branch): 4-5 cycles
 *   - Loop exit (fall through): 3-4 cycles
 *   - Slightly slower than LOOPI/LOOPD due to extra operand
 *
 * Key Characteristics:
 *   - Atomic add-compare-branch operation
 *   - Signed comparison (index <= limit continues loop)
 *   - Configurable step size (positive, negative, or any value)
 *   - No flags modified
 *   - Four operands (most complex loop instruction)
 *   - Supports byte and halfword displacement
 *   - PC-relative branching (not absolute)
 *   - Most flexible loop instruction
 *   - Index is read-write operand (modified in place)
 *
 * Common Use Cases:
 *   - Strided array access (every Nth element)
 *   - Multi-dimensional array traversal
 *   - Custom step size loops
 *   - Loops with non-unit increment/decrement
 *   - Processing data with specific spacing
 *   - Matrix row/column iteration
 *   - Pointer arithmetic with custom stride
 *
 * Example Usage:
 *   ; Process every 4th element (stride 4)
 *   I1 = 0                     ; Start at index 0
 *   STRIDE_LOOP:
 *       I2 = ARRAY(I1)         ; Load element
 *       PROCESS I2             ; Process it
 *       W LOOP:B I1, 4, ARRAY_SIZE-1, STRIDE_LOOP  ; Step by 4
 *
 *   ; Countdown by 2 (even numbers)
 *   I1 = 100                   ; Start at 100
 *   COUNTDOWN:
 *       PROCESS I1             ; Process even value
 *       W LOOP:B I1, -2, 0, COUNTDOWN  ; Decrement by 2
 *
 *   ; Matrix column access (stride = row_size)
 *   I1 = COLUMN_INDEX          ; Starting column
 *   I2 = ROW_SIZE              ; Step size
 *   COLUMN_LOOP:
 *       I3 = MATRIX(I1)        ; Access element
 *       PROCESS I3
 *       W LOOP:H I1, I2, MAX_OFFSET, COLUMN_LOOP
 *
 *   ; Custom increment loop
 *   I1 = 0                     ; Index
 *   I2 = 3                     ; Custom step
 *   CUSTOM_LOOP:
 *       ; Process at index I1
 *       WORK I1
 *       W LOOP:B I1, I2, 99, CUSTOM_LOOP  ; Loop with step=3
 *
 *   ; Backward stride
 *   I1 = ARRAY_SIZE - 1        ; Start at end
 *   BACK_STRIDE:
 *       I2 = ARRAY(I1)
 *       PROCESS I2
 *       W LOOP:B I1, -4, 0, BACK_STRIDE  ; Backward by 4
 *
 * Related Instructions:
 *   - LOOPI: Loop with increment (+1 step, 3 operands)
 *   - LOOPD: Loop with decrement (-1 step, 3 operands)
 *   - IF conditions: Simple conditional branches
 *   - GO: Unconditional relative branch
 *
 * Comparison with Related Instructions:
 *   - LOOP vs LOOPI: Configurable step vs fixed +1
 *   - LOOP vs LOOPD: Configurable step vs fixed -1
 *   - LOOP vs both: Extra operand, no flag modification
 *   - LOOP vs ADD+IF: Combines operations, more efficient
 *
 * Typical Pattern (Strided Loop):
 *   ; Initialize index and step
 *   I1 = START_INDEX
 *   I2 = STRIDE
 *
 *   LOOP_BODY:
 *       ; Use index I1
 *       PROCESS ARRAY(I1)
 *
 *       ; Add step and loop
 *       W LOOP:B I1, I2, LIMIT, LOOP_BODY
 *
 *   ; Loop exits when I1 > LIMIT
 *
 * Loop Termination:
 *   The loop terminates when (index + step) > limit
 *   - For count-up: step > 0, limit is max value
 *   - For countdown: step < 0, limit is min value
 *   - Be careful with step=0 (infinite loop)
 *
 * Infinite Loop Warning:
 *   If step = 0, the index never changes and the loop runs forever
 *   (assuming index <= limit initially). Always ensure step != 0.
 *
 * Signed vs Unsigned:
 *   LOOP uses signed comparison. For unsigned loops with large positive
 *   values, ensure step and limit are chosen to avoid signed overflow.
 *
 * Float/Double Support:
 *   Float and Double variants exist for floating-point loop counters.
 *   They perform floating-point addition and comparison (not yet implemented).
 *
 * Comparison with C-style Loops:
 *   C:        for (i = 0; i < N; i += STEP)
 *   ND-500:   I1 = 0
 *             I2 = STEP
 *             LOOP: ...body...
 *             W LOOP:B I1, I2, N-1, LOOP
 *
 *   Note: Limit is N-1 because LOOP uses <= comparison, not <
 *
 * Multi-dimensional Array Access:
 *   For 2D arrays stored row-major:
 *   - Row iteration: step = row_size
 *   - Column iteration: step = 1
 *   - Diagonal: step = row_size + 1
 *
 * Performance Considerations:
 *   LOOP is slightly slower than LOOPI/LOOPD due to the extra operand read.
 *   For simple +1/-1 loops, prefer LOOPI/LOOPD for better performance.
 *   Use LOOP when step is variable or non-unit.
 */
void nd500_instr_Loop(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count
    if (fi->operand_count != 4) {
        printf("[ERROR] LOOP at PC=0x%08X: Expected 4 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    bool should_loop = false;

    // Handle float/double variants
    if (fi->data_type == ND500_DTYPE_FLOAT || fi->data_type == ND500_DTYPE_DOUBLEWORD) {
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);

        // Read operands as IEEE-754 floats
        double fp_index = nd500_read_operand_as_ieee_float(cpu, &fi->operands[0], is_double);
        double fp_step = nd500_read_operand_as_ieee_float(cpu, &fi->operands[1], is_double);
        double fp_limit = nd500_read_operand_as_ieee_float(cpu, &fi->operands[2], is_double);

        // Add step to index
        double fp_new_index = fp_index + fp_step;

        // Write back updated index
        nd500_write_operand_from_ieee_float(cpu, &fi->operands[0], fp_new_index, is_double);

        // Compare: loop if new_index <= limit
        should_loop = (fp_new_index <= fp_limit);
    } else {
        // Integer variants
        uint64_t index = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
        uint64_t step = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);
        uint64_t limit = nd500_read_operand_value(cpu, &fi->operands[2], fi->data_type);

        // Add step to index
        uint64_t new_index = index + step;

        // Write updated index back
        nd500_write_operand_value(cpu, &fi->operands[0], new_index, fi->data_type);

        // Perform signed comparison based on data type
        switch (fi->data_type) {
            case ND500_DTYPE_BYTE:
                should_loop = ((int8_t)new_index <= (int8_t)limit);
                break;
            case ND500_DTYPE_HALFWORD:
                should_loop = ((int16_t)new_index <= (int16_t)limit);
                break;
            case ND500_DTYPE_WORD:
                should_loop = ((int32_t)new_index <= (int32_t)limit);
                break;
            default:
                printf("[ERROR] LOOP at PC=0x%08X: Invalid data type %u\n",
                       fi->address, fi->data_type);
                trap_invalid_operation(cpu, fi->address);
                return;
        }
    }

    if (should_loop) {
        // Jump back to start of loop (PC + displacement -> PC)
        // Displacement is always read as the instruction's displacement type (byte or halfword)
        uint64_t value = nd500_read_operand_value(cpu, &fi->operands[3], fi->data_type);
        int64_t displacement = nd500_sign_extend_by_dtype(value, fi->data_type);
        cpu->PC = (uint32_t)(fi->address + displacement);
    }
    // else: fall through to next instruction

    // Note: LOOP does not modify any status flags
}
