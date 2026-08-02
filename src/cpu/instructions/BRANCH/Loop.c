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

    // Determine data type and displacement width from opcode
    // fi->data_type is wrong for LOOP because variant encodes both data type AND disp width
    // LOOP opcodes:
    //   0xFD2D=BY:B, 0xFD32=BY:H, 0xFD2E=H:B, 0xFD33=H:H, 0xFD2F=W:B, 0xFD34=W:H
    //   0xFD30=F:B,  0xFD35=F:H,  0xFD31=D:B, 0xFD36=D:H
    uint8_t opcode_low = fi->opcode & 0xFF;
    bool is_halfword_disp = (opcode_low >= 0x32);

    // Map opcode to data type
    // :B variants: 2D=BY, 2E=H, 2F=W, 30=F, 31=D
    // :H variants: 32=BY, 33=H, 34=W, 35=F, 36=D
    Nd500DataType data_type;
    bool is_float_type = false;
    if (is_halfword_disp) {
        // :H variants start at 0x32
        switch (opcode_low) {
            case 0x32: data_type = ND500_DTYPE_BYTE; break;
            case 0x33: data_type = ND500_DTYPE_HALFWORD; break;
            case 0x34: data_type = ND500_DTYPE_WORD; break;
            case 0x35: data_type = ND500_DTYPE_WORD; is_float_type = true; break;  // F
            case 0x36: data_type = ND500_DTYPE_DOUBLEWORD; is_float_type = true; break;  // D
            default:   data_type = ND500_DTYPE_WORD; break;
        }
    } else {
        // :B variants start at 0x2D
        switch (opcode_low) {
            case 0x2D: data_type = ND500_DTYPE_BYTE; break;
            case 0x2E: data_type = ND500_DTYPE_HALFWORD; break;
            case 0x2F: data_type = ND500_DTYPE_WORD; break;
            case 0x30: data_type = ND500_DTYPE_WORD; is_float_type = true; break;  // F
            case 0x31: data_type = ND500_DTYPE_DOUBLEWORD; is_float_type = true; break;  // D
            default:   data_type = ND500_DTYPE_WORD; break;
        }
    }

    bool should_loop = false;

    // Handle float/double variants
    if (is_float_type) {
        bool is_double = (data_type == ND500_DTYPE_DOUBLEWORD);

        // For F/D LOOP, index is read as float from register/memory
        // but step and limit may be integer constants that need conversion
        double fp_index = nd500_read_operand_as_ieee_float(cpu, &fi->operands[0], is_double);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }

        // Step and limit: if constant operand, convert integer to float
        double fp_step, fp_limit;
        if (fi->operands[1].mode == ND500_ADDR_CONSTANT_SHORT ||
            fi->operands[1].mode == ND500_ADDR_CONSTANT) {
            // Integer constant - convert to float
            int64_t step_int = nd500_sign_extend_by_dtype(
                nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_WORD),
                ND500_DTYPE_WORD);
            /* A faulting operand read must abort the instruction: commit nothing,
             * and raise no second trap on top of the fault the kernel is already
             * about to service. See the ADD3 guard (commit a351296) for the panic
             * this prevents. */
            if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
            }
            fp_step = (double)step_int;
        } else {
            fp_step = nd500_read_operand_as_ieee_float(cpu, &fi->operands[1], is_double);
            /* A faulting operand read must abort the instruction: commit nothing,
             * and raise no second trap on top of the fault the kernel is already
             * about to service. See the ADD3 guard (commit a351296) for the panic
             * this prevents. */
            if (nd500_trap_occurred() || cpu->instr_aborted) {
                return;
            }
        }

        if (fi->operands[2].mode == ND500_ADDR_CONSTANT_SHORT ||
            fi->operands[2].mode == ND500_ADDR_CONSTANT) {
            // Integer constant - convert to float
            int64_t limit_int = nd500_sign_extend_by_dtype(
                nd500_read_operand_value(cpu, &fi->operands[2], ND500_DTYPE_WORD),
                ND500_DTYPE_WORD);
            /* A faulting operand read must abort the instruction: commit nothing,
             * and raise no second trap on top of the fault the kernel is already
             * about to service. See the ADD3 guard (commit a351296) for the panic
             * this prevents. */
            if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
            }
            fp_limit = (double)limit_int;
        } else {
            fp_limit = nd500_read_operand_as_ieee_float(cpu, &fi->operands[2], is_double);
            /* A faulting operand read must abort the instruction: commit nothing,
             * and raise no second trap on top of the fault the kernel is already
             * about to service. See the ADD3 guard (commit a351296) for the panic
             * this prevents. */
            if (nd500_trap_occurred() || cpu->instr_aborted) {
                return;
            }
        }

        // Any of the three operand reads above can page-fault when the operand
        // lives in memory. raise_trap dispatches the fault synchronously, so
        // continuing would write a garbage index BACK TO MEMORY and then clobber
        // the freshly installed trap-handler PC. See the same guard on the
        // integer path below, and JUMPG (dc2640c) for the proven case.
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }

        // Add step to index
        double fp_new_index = fp_index + fp_step;

        // Write back updated index
        nd500_write_operand_from_ieee_float(cpu, &fi->operands[0], fp_new_index, is_double);

        // Per C# reference: exit if (step > 0 && newIndex > limit) || (step < 0 && newIndex < limit)
        double diff = fp_new_index - fp_limit;
        bool exit_loop = (fp_step > 0 && diff > 0) || (fp_step < 0 && diff < 0);
        should_loop = !exit_loop;
    } else {
        // Integer variants.
        //
        // The index is read and written at the instruction's own data type, like
        // <step> and <limit> below. ND-05.009.4 section 13.6: "The <index>,
        // <step> and <limit> operands are of the same data type, which may be
        // BY, H, W, F or D." Forcing WORD corrupts neighbouring bytes whenever
        // the index is a memory operand - see the LOOPI fix and
        // docs/HANDOFF_LOOPI_INDEX_DATATYPE.md, where exactly that desynchronised
        // the ND linker's NRF record scanner.
        uint64_t index = nd500_read_operand_value(cpu, &fi->operands[0], data_type);
        uint64_t step = nd500_read_operand_value(cpu, &fi->operands[1], data_type);
        uint64_t limit = nd500_read_operand_value(cpu, &fi->operands[2], data_type);

        // Any of the three reads above can page-fault when the operand lives in
        // memory. raise_trap dispatches the fault synchronously - installing the
        // handler PC and clearing the trap state - so continuing would write a
        // garbage index BACK TO MEMORY below and then clobber that handler PC.
        // Aborting is restart-safe: nothing has been committed yet and LOOP
        // re-executes after the handler RETTs. Proven case: JUMPG (dc2640c).
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }

        // step == 0 -> Illegal Operand Value (IOV) trap, then fall through to the
        // next instruction (no index update, no loop-back). Manual 13.6; microcode
        // 000637 -> LOOP_IOV_B. Without this a zero step is an INFINITE LOOP.
        bool step_zero = false;
        switch (data_type) {
            case ND500_DTYPE_BYTE:     step_zero = ((int8_t)step == 0); break;
            case ND500_DTYPE_HALFWORD: step_zero = ((int16_t)step == 0); break;
            case ND500_DTYPE_WORD:     step_zero = ((int32_t)step == 0); break;
            default: break;
        }
        if (step_zero) {
            raise_trap(cpu, TRAP_IOV, fi->address, 0);  // Illegal Operand Value
            // Fall through to the next sequential instruction (no loop-back).
            uint32_t actual_len = 2;  // opcode
            for (int i = 0; i < 3; i++) {
                actual_len += 1 + fi->operands[i].data_len;
            }
            actual_len += is_halfword_disp ? 2 : 1;
            cpu->PC = fi->address + actual_len;
            return;
        }

        // Add step to index (full 32-bit operation)
        uint64_t new_index = index + step;

        // Write updated index back at the instruction's data type
        nd500_write_operand_value(cpu, &fi->operands[0], new_index, data_type);

        // Per C# reference:
        // exit if (step > 0 && newIndex > limit) || (step < 0 && newIndex < limit)
        // Signed comparison based on data type
        bool exit_loop = false;
        switch (data_type) {
            case ND500_DTYPE_BYTE: {
                int8_t s_step = (int8_t)step;
                int8_t s_new = (int8_t)new_index;
                int8_t s_limit = (int8_t)limit;
                exit_loop = (s_step > 0 && s_new > s_limit) || (s_step < 0 && s_new < s_limit);
                break;
            }
            case ND500_DTYPE_HALFWORD: {
                int16_t s_step = (int16_t)step;
                int16_t s_new = (int16_t)new_index;
                int16_t s_limit = (int16_t)limit;
                exit_loop = (s_step > 0 && s_new > s_limit) || (s_step < 0 && s_new < s_limit);
                break;
            }
            case ND500_DTYPE_WORD: {
                int32_t s_step = (int32_t)step;
                int32_t s_new = (int32_t)new_index;
                int32_t s_limit = (int32_t)limit;
                exit_loop = (s_step > 0 && s_new > s_limit) || (s_step < 0 && s_new < s_limit);
                break;
            }
            default:
                printf("[ERROR] LOOP at PC=0x%08X: Invalid data type %u\n",
                       fi->address, data_type);
                trap_invalid_operation(cpu, fi->address);
                return;
        }
        should_loop = !exit_loop;
    }

    if (should_loop) {
        // Jump back to start of loop (PC + displacement -> PC)
        const Nd500OperandDecoded* disp_op = &fi->operands[3];

        int64_t displacement;
        if (is_halfword_disp) {
            // :H variant - 2-byte signed displacement (big-endian)
            int16_t disp16 = (int16_t)((disp_op->data[0] << 8) | disp_op->data[1]);
            displacement = disp16;
        } else {
            // :B variant - 1-byte signed displacement
            displacement = (int8_t)disp_op->data[0];
        }

        cpu->PC = (uint32_t)(fi->address + displacement);
    } else {
        // Loop exit - PC advances to next instruction
        // Can't rely on fi->total_len because decoder has wrong data_type
        // Calculate actual length: 2 (opcode) + operand sizes
        // Operands 0-2 use address codes, operand 3 is displacement
        // For typical case with register + 2 short constants: 2 + 1 + 1 + 1 + disp
        uint32_t actual_len = 2;  // opcode
        for (int i = 0; i < 3; i++) {
            // Each operand is at least 1 byte (address code) plus any data bytes
            actual_len += 1 + fi->operands[i].data_len;
        }
        // Displacement operand length (no address code, just data)
        actual_len += is_halfword_disp ? 2 : 1;
        cpu->PC = fi->address + actual_len;
    }

    // Note: LOOP does not modify any status flags
}
