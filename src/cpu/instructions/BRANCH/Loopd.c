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

    bool should_loop = false;
    uint64_t index_bits = 0;  // For flag updates

    // Handle float/double variants
    // Note: Decoder sets data_type=WORD for F variants, so check uses_float_registers instead
    if (fi->uses_float_registers) {
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);

        // Read index operand as IEEE-754 float
        double fp_index = nd500_read_operand_as_ieee_float(cpu, &fi->operands[0], is_double);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }

        // Limit: if constant operand, convert integer to float
        double fp_limit;
        if (fi->operands[1].mode == ND500_ADDR_CONSTANT_SHORT ||
            fi->operands[1].mode == ND500_ADDR_CONSTANT) {
            // Integer constant - convert to float
            int64_t limit_int = nd500_sign_extend_by_dtype(
                nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_WORD),
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
            fp_limit = nd500_read_operand_as_ieee_float(cpu, &fi->operands[1], is_double);
            /* A faulting operand read must abort the instruction: commit nothing,
             * and raise no second trap on top of the fault the kernel is already
             * about to service. See the ADD3 guard (commit a351296) for the panic
             * this prevents. */
            if (nd500_trap_occurred() || cpu->instr_aborted) {
                return;
            }
        }

        // Decrement by 1.0
        fp_index -= 1.0;

        // Write back updated index
        nd500_write_operand_from_ieee_float(cpu, &fi->operands[0], fp_index, is_double);

        // Set Z/S flags based on modified index
        union { float f; uint32_t u; } conv_f;
        union { double d; uint64_t u; } conv_d;
        if (is_double) {
            conv_d.d = fp_index;
            nd500_set_flags_zs_float(cpu, conv_d.u, true);
        } else {
            conv_f.f = (float)fp_index;
            nd500_set_flags_zs_float(cpu, conv_f.u, false);
        }

        // Compare: loop if index >= limit (countdown)
        should_loop = (fp_index >= fp_limit);
        (void)index_bits;  // Suppress unused variable warning
    } else {
        // Integer variants.
        //
        // The index is read and written at the instruction's own data type, not
        // forced to WORD. ND-05.009.4 section 13.5: "The <index> and <limit>
        // operands are of the same data type, which may be BY, H, W, F or D."
        // Forcing WORD corrupts neighbouring bytes whenever the index is a
        // memory operand - see docs/HANDOFF_LOOPI_INDEX_DATATYPE.md, where
        // exactly that desynchronised the ND linker's NRF record scanner.
        uint64_t index = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
        uint64_t limit = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

        // Either read can page-fault when the operand lives in memory. The fault
        // is dispatched synchronously, so continuing would write a garbage index
        // BACK TO MEMORY below and then clobber the installed trap-handler PC.
        // Restart-safe: nothing committed yet. Proven case: JUMPG (dc2640c).
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }

        // Decrement index by 1
        index--;

        // Write decremented index back at the instruction's data type
        nd500_write_operand_value(cpu, &fi->operands[0], index, fi->data_type);

        // Integer LOOPD DOES set Z/S from the decremented index at the datatype width: the
        // microcode LOOPDB @000615 / LOOPDH @000621 run ST,SAVA. (The earlier code left the flags.)
        // Adjudicated microword-right; matches RetroCore Loopd.cs SetStatusZS(index, dataType).
        // [LOOPD Z/S 2026-07-27]
        nd500_set_flags_zs(cpu, index, fi->data_type);

        // Perform signed comparison based on data type (compare low bits only)
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
            default:
                printf("[ERROR] LOOPD at PC=0x%08X: Invalid data type %u\n",
                       fi->address, fi->data_type);
                trap_invalid_operation(cpu, fi->address);
                return;
        }

    }

    // Displacement size is determined by the opcode variant:
    //   :B variants (0xFD23-0xFD27): 1-byte signed displacement
    //   :H variants (0xFD28-0xFD2C): 2-byte signed displacement
    bool is_halfword_disp = (fi->opcode >= 0xFD28 && fi->opcode <= 0xFD2C);

    if (should_loop) {
        // Jump back to start of loop (PC + displacement -> PC)
        // NOTE: Read directly from operand data[], not via nd500_read_operand_value,
        // because the decoder incorrectly sets data_len based on data_type instead of opcode
        const Nd500OperandDecoded* disp_op = &fi->operands[2];

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
        // BT flag (bit 18) - only set for F/D variants when branch is taken
        if (fi->uses_float_registers) {
            cpu->ST1 |= 0x40000;
        }
    } else {
        // Loop exit - PC advances to next instruction
        // Can't rely on fi->total_len because decoder uses wrong data_type for displacement
        // Calculate actual length: opcode + operand sizes + displacement
        uint32_t actual_len = fi->opcode_len;  // opcode (2 bytes for LOOPD)
        for (int i = 0; i < 2; i++) {
            // Each operand is at least 1 byte (address code) plus any data bytes
            actual_len += 1 + fi->operands[i].data_len;
        }
        // Displacement operand length (no address code, just data)
        actual_len += is_halfword_disp ? 2 : 1;
        cpu->PC = fi->address + actual_len;
    }
}
