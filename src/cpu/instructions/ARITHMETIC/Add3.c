#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <math.h>

/**
 * Add3 instruction - ARITHMETIC class
 *
 * Extended Add (Three Operands): <a> + <b> → <c>
 *
 * Variants: 5 (by data type and register)
 * Mnemonics: BYn ADD3, Hn ADD3, Wn ADD3, Fn ADD3, Dn ADD3 (n=1..4)
 * Operands: 3 (<a/r/t>, <b/r/t>, <c/w/t>)
 *
 * Opcodes:
 *   0xFC44-0xFC47 (BY1 ADD3 through BY4 ADD3) - Byte extended add
 *   0xFC48-0xFC4B (H1 ADD3 through H4 ADD3) - Halfword extended add
 *   0x006C-0x006F (W1 ADD3 through W4 ADD3) - Word extended add
 *   0x0070-0x0073 (F1 ADD3 through F4 ADD3) - Float extended add
 *   0x0074-0x0077 (D1 ADD3 through D4 ADD3) - Double extended add
 *
 * Operation: <a> + <b> → <c>
 *
 * Description:
 *   The <a> operand is added to the <b> operand and the result is stored
 *   in the <c> operand location. This is a three-operand version that allows
 *   addition without affecting any register contents.
 *
 * Flags: Z (zero), S (sign), C (carry), O (overflow)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   C = 1 if carry from most significant bit (integer only)
 *   O = 1 if overflow
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *
 * Reference: ND-500 Reference Manual, Chapter 11 (Extended Arithmetic)
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Add3.cs
 */
void nd500_instr_Add3(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 54-58) */
    if (fi->operand_count != 3) {
        printf("[ERROR] ADD3 at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Handle float/double types.
     * Microcode ADD3F @002177 / ADD3D @002221: READ a, READ b, a+b -> SC1 (ST,SAVF),
     * WRITE sum to <c>. Float status = Z,S,FU,FO; C and O are not named by the
     * instruction and are therefore CLEARED (Reference rule 4040). */
    if (fi->uses_float_registers) {
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);

        /* READ operand a and operand b as IEEE doubles */
        double aValue = nd500_read_operand_as_ieee_float(cpu, &fi->operands[0], is_double);
        double bValue = nd500_read_operand_as_ieee_float(cpu, &fi->operands[1], is_double);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }

        /* a + b -> <c>; shared tail sets Z,S / clears C,O / sets+traps FU,FO */
        double fresult = aValue + bValue;
        nd500_write_operand_from_ieee_float(cpu, &fi->operands[2], fresult, is_double);
        nd500_float_finish(cpu, fi->address, fresult, is_double);
        return;
    }

    uint64_t aValue, bValue, result;
    bool overflow = false;
    bool carry = false;

    /* Read operand a value (like C# line 61) */
    aValue = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Read operand b value (like C# line 64) */
    bValue = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

    /* An operand read that page-faulted leaves aValue/bValue undefined, and
     * the instruction must commit NOTHING: no result write, no flags, and
     * above all no second trap. Without this guard ADD3 ran on to write a
     * garbage result and then raise "Integer overflow" on top of the pending
     * page fault, and NDIX panicked "pagein valid page" (sys/vm_page.c:118)
     * the moment cc was run in the guest - pagein() was entered for a page
     * whose pte already had a frame, because the spurious second trap arrived
     * against a fault the kernel had already serviced.
     *
     * Same defect class as the ENT* abort guards (see the CALL/ENT* interlock
     * work): a faulting mid-instruction access must abort the instruction, not
     * merely record a trap and carry on. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Perform addition: a + b (like C# lines 66-103) */
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE: {
            /* Signed byte addition (like C# lines 73-81) */
            int8_t aByte = (int8_t)(aValue & 0xFF);
            int8_t bByte = (int8_t)(bValue & 0xFF);
            int32_t sum = (int32_t)aByte + (int32_t)bByte;
            result = (uint64_t)(uint8_t)(sum & 0xFF);
            overflow = (sum < -128 || sum > 127);
            /* Carry is UNSIGNED overflow - must use the unsigned operands, not
             * the signed sum (a negative signed sum is sign-extended, so its
             * bit 8 would be set even with no real unsigned carry). */
            carry = (((uint32_t)(aValue & 0xFF) + (uint32_t)(bValue & 0xFF)) & 0x100u) != 0;
            break;
        }

        case ND500_DTYPE_HALFWORD: {
            /* Signed halfword addition (like C# lines 84-92) */
            int16_t aHalf = (int16_t)(aValue & 0xFFFF);
            int16_t bHalf = (int16_t)(bValue & 0xFFFF);
            int32_t sum = (int32_t)aHalf + (int32_t)bHalf;
            result = (uint64_t)(uint16_t)(sum & 0xFFFF);
            overflow = (sum < -32768 || sum > 32767);
            /* Carry from UNSIGNED addition (see BYTE case). */
            carry = (((uint32_t)(aValue & 0xFFFF) + (uint32_t)(bValue & 0xFFFF)) & 0x10000u) != 0;
            break;
        }

        case ND500_DTYPE_WORD: {
            /* Signed word addition (like C# lines 95-103) */
            int32_t aWord = (int32_t)(aValue & 0xFFFFFFFF);
            int32_t bWord = (int32_t)(bValue & 0xFFFFFFFF);
            int64_t sum = (int64_t)aWord + (int64_t)bWord;
            result = (uint64_t)(uint32_t)(sum & 0xFFFFFFFF);
            overflow = (sum < INT32_MIN || sum > INT32_MAX);
            /* Carry from UNSIGNED addition (see BYTE case). Using the signed
             * sum broke the NDIX kernel's fuword: add3 $_Udata(0xF0000000),
             * uaddr -> 0xF8000014 has NO unsigned carry, but the signed sum is
             * negative and its bit 32 (sign extension) set a false carry, so
             * `if >>= go _fuerror` wrongly took the fault path and every
             * syscall arg read returned EFAULT. */
            carry = (((uint64_t)(uint32_t)(aValue & 0xFFFFFFFF)
                    + (uint64_t)(uint32_t)(bValue & 0xFFFFFFFF)) & 0x100000000ULL) != 0;
            break;
        }

        default:
            printf("[ERROR] ADD3 at PC=0x%08X: Unsupported data type %d\n",
                   fi->address, fi->data_type);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    /* Write result to operand c location (like C# line 144) */
    nd500_write_operand_value(cpu, &fi->operands[2], result, fi->data_type);

    /* Update status flags based on result (like C# lines 146-150) */
    nd500_set_flags_zsco(cpu, result, fi->data_type, carry, overflow);

    /* Handle trap conditions (like C# lines 152-164) */
    if (overflow) {
        printf("[TRAP] ADD3 at PC=0x%08X: Integer overflow\n", fi->address);
        trap_invalid_operation(cpu, fi->address);
        return;
    }
}
