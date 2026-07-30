#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Loopi instruction - BRANCH class
 *
 * LOOPI - Loop with Increment (Increment index and conditionally branch)
 *
 * Mnemonic: LOOPI
 * Format: t LOOPI <index/rw/t>, <limit/r/t>, <<displacement>>
 * Variants: 10 (BY, H, W, F, D with B/H displacement)
 * Operands: 3 (<index/rw>, <limit/r>, <<displacement>>)
 *
 * Opcodes:
 *   0xFCDE (BY LOOPI:B) - Byte index, byte displacement
 *   0xFD1E (BY LOOPI:H) - Byte index, halfword displacement
 *   0xFCDF (H LOOPI:B)  - Halfword index, byte displacement
 *   0xFD1F (H LOOPI:H)  - Halfword index, halfword displacement
 *   0x00BF (W LOOPI:B)  - Word index, byte displacement
 *   0x00E1 (W LOOPI:H)  - Word index, halfword displacement
 *   0xFD1C (F LOOPI:B)  - Float index, byte displacement
 *   0xFD21 (F LOOPI:H)  - Float index, halfword displacement
 *   0xFD1D (D LOOPI:B)  - Double index, byte displacement
 *   0xFD22 (D LOOPI:H)  - Double index, halfword displacement
 *
 * Operation:
 *   <index> + 1 -> <index>
 *   if (<index> - <limit>) > 0 then
 *       address of next instruction -> PC (fall through)
 *   else
 *       PC + <<displacement>> -> PC (loop back)
 *   endif
 *
 * Description:
 *   Implements a loop with incrementing index counter. The instruction first
 *   increments the index operand by one, writes it back, then compares the
 *   modified index with the limit value.
 *
 *   If the incremented index is less than or equal to the limit (signed
 *   comparison), the loop continues by adding the signed displacement to PC,
 *   typically jumping backwards to the start of the loop body.
 *
 *   If the incremented index is greater than the limit, the loop terminates
 *   and execution falls through to the next instruction.
 *
 *   This instruction is typically placed at the end of a loop body with a
 *   negative displacement value to jump back to the beginning. The loop runs
 *   while index <= limit, enabling count-up loops.
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
 *   Note: Flags reflect the incremented index value, not the comparison result
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
 *   - Atomic increment-compare-branch operation
 *   - Signed comparison (index <= limit continues loop)
 *   - Increments before comparison
 *   - Z/S flags reflect incremented index
 *   - Supports byte and halfword displacement
 *   - PC-relative branching (not absolute)
 *   - Ideal for count-up loops
 *   - Index is read-write operand (modified in place)
 *
 * Common Use Cases:
 *   - Count-up loops (for i = 0 to N)
 *   - Array traversal from low to high index
 *   - Forward iteration over data structures
 *   - Loop counters that increment to limit
 *   - Sequential data processing
 *   - String scanning forward
 *
 * Example Usage:
 *   ; Count-up loop from 0 to 9
 *   I1 = 0                     ; Initialize index
 *   LOOP_START:
 *       ; Loop body code here
 *       PROCESS I1             ; Use index value
 *       W LOOPI:B I1, 9, LOOP_START  ; Increment and loop if <= 9
 *   ; Falls through when I1 reaches 10
 *
 *   ; Array traversal from start to end
 *   I2 = 0                     ; Start at first element
 *   TRAVERSE_LOOP:
 *       I3 = ARRAY(I2)         ; Load element
 *       PROCESS I3             ; Process element
 *       W LOOPI:B I2, ARRAY_SIZE-1, TRAVERSE_LOOP  ; Loop through all
 *
 *   ; Count-up with memory operand
 *   COUNTER = 0                ; Initialize counter
 *   WORK_LOOP:
 *       ; Perform work
 *       DO_WORK
 *       W LOOPI:H COUNTER, 99, WORK_LOOP  ; Loop 100 times (0-99)
 *
 *   ; Forward string scan
 *   I1 = 0                     ; Start at beginning
 *   SCAN_LOOP:
 *       I2 = STRING(I1)
 *       COMP I2, TARGET_CHAR
 *       IF=GO:B FOUND
 *       BY LOOPI:B I1, STRING_LEN-1, SCAN_LOOP
 *   NOT_FOUND:
 *       ; Character not found
 *   FOUND:
 *       ; Character found at index I1
 *
 * Related Instructions:
 *   - LOOPD: Loop with decrement (counts down)
 *   - LOOP: General loop with configurable step
 *   - IF conditions: Simple conditional branches
 *   - GO: Unconditional relative branch
 *
 * Comparison with Related Instructions:
 *   - LOOPI vs LOOPD: Increments vs decrements, <= vs >= comparison
 *   - LOOPI vs LOOP: Fixed +1 step vs configurable step
 *   - LOOPI vs IF>GO: Combined increment+compare vs separate operations
 *
 * Typical Pattern (Count-Up Loop):
 *   ; Initialize counter
 *   I1 = 0
 *
 *   LOOP_BODY:
 *       ; Use counter value
 *       PROCESS I1
 *
 *       ; Increment and loop
 *       W LOOPI:B I1, N-1, LOOP_BODY
 *
 *   ; Loop exits when I1 > N-1 (i.e., I1 reaches N)
 *
 * Loop Termination:
 *   The loop terminates when (index + 1) > limit
 *   For count to N: use limit = N-1 to execute loop body N times
 *
 * Signed vs Unsigned:
 *   LOOPI uses signed comparison. For unsigned count-up loops, ensure the
 *   index never becomes negative (start at 0 or positive value).
 *
 * Float/Double Support:
 *   Float and Double variants exist but are not commonly used. They perform
 *   floating-point increment by 1.0 and floating-point comparison.
 *
 * Comparison with C-style Loops:
 *   C:        for (i = 0; i < N; i++)
 *   ND-500:   I1 = 0
 *             LOOP: ...body...
 *             W LOOPI:B I1, N-1, LOOP
 *
 *   Note: Limit is N-1 because LOOPI uses <= comparison, not <
 */
void nd500_instr_Loopi(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count
    if (fi->operand_count != 3) {
        printf("[ERROR] LOOPI at PC=0x%08X: Expected 3 operands, got %u\n",
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

        // Limit: if constant operand, convert integer to float
        double fp_limit;
        if (fi->operands[1].mode == ND500_ADDR_CONSTANT_SHORT ||
            fi->operands[1].mode == ND500_ADDR_CONSTANT) {
            // Integer constant - convert to float
            int64_t limit_int = nd500_sign_extend_by_dtype(
                nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_WORD),
                ND500_DTYPE_WORD);
            fp_limit = (double)limit_int;
        } else {
            fp_limit = nd500_read_operand_as_ieee_float(cpu, &fi->operands[1], is_double);
        }

        // Increment by 1.0
        fp_index += 1.0;

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

        // Compare: loop if index <= limit
        should_loop = (fp_index <= fp_limit);
        (void)index_bits;  // Suppress unused variable warning
    } else {
        // Integer variants.
        //
        // The index MUST be read and written at the instruction's own data type,
        // not forced to WORD. Per the ND-500 Reference Manual (13.4 Loop with
        // increment): "The <index> and <limit> operands are of the same data
        // type, which may be BY, H, W, F or D."
        //
        // Forcing WORD is wrong for any operand in MEMORY. The ND linker's NRF
        // record scanner runs
        //     B001D816: h stz    b.0x14        ; index = 0   (2 bytes)
        //     B001D813: by2 =:   b.0x16        ; unrelated byte, adjacent
        //     B001D891: FC DF 45 48 93         ; H LOOPI:B b.0x14, b.0x20, -109
        // A WORD read of b.0x14 also swallows b.0x16, so the index came back as
        // 0x00000800 instead of 0, incremented to 0x0801, and (int16_t)0x0801 =
        // 2049 > limit 1 ended the loop after ONE iteration instead of two. The
        // scanner then consumed one length byte too few, desynchronised from the
        // record stream, and the linker rejected every object file with
        // '*** ERROR - "4" in module  is illegal control byte. (0054:16)'.
        // The WORD write-back additionally clobbered the neighbouring b.0x16.
        uint64_t index = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
        uint64_t limit = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

        // Either read can page-fault when the operand lives in memory. The fault
        // is dispatched synchronously, so continuing would write a garbage index
        // BACK TO MEMORY below and then clobber the installed trap-handler PC.
        // Restart-safe: nothing committed yet. Proven case: JUMPG (dc2640c).
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }

        // Increment index by 1
        index++;

        // Write incremented index back at the instruction's data type
        nd500_write_operand_value(cpu, &fi->operands[0], index, fi->data_type);

        // Perform signed comparison based on data type (compare low bits only)
        switch (fi->data_type) {
            case ND500_DTYPE_BYTE:
                should_loop = ((int8_t)index <= (int8_t)limit);
                break;
            case ND500_DTYPE_HALFWORD:
                should_loop = ((int16_t)index <= (int16_t)limit);
                break;
            case ND500_DTYPE_WORD:
                should_loop = ((int32_t)index <= (int32_t)limit);
                break;
            default:
                printf("[ERROR] LOOPI at PC=0x%08X: Invalid data type %u\n",
                       fi->address, fi->data_type);
                trap_invalid_operation(cpu, fi->address);
                return;
        }

        // Note: LOOPI does NOT modify status flags per ND-500 Reference Manual
        // "Data status bits: Unaffected"
    }

    // Displacement size is determined by the opcode variant, NOT the data type:
    //   :B variants: 0xFCDE, 0xFCDF, 0x00BF, 0xFD1C, 0xFD1D (1-byte signed displacement)
    //   :H variants: 0xFD1E, 0xFD1F, 0x00E1, 0xFD21, 0xFD22 (2-byte signed displacement)
    bool is_halfword_disp = (fi->opcode == 0xFD1E || fi->opcode == 0xFD1F ||
                             fi->opcode == 0x00E1 || fi->opcode == 0xFD21 ||
                             fi->opcode == 0xFD22);

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
        uint32_t actual_len = fi->opcode_len;  // opcode (1 or 2 bytes)
        for (int i = 0; i < 2; i++) {
            // Each operand is at least 1 byte (address code) plus any data bytes
            actual_len += 1 + fi->operands[i].data_len;
        }
        // Displacement operand length (no address code, just data)
        actual_len += is_halfword_disp ? 2 : 1;
        cpu->PC = fi->address + actual_len;
    }
}
