#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Lind instruction - SYSTEM class
 *
 * LIND - Load Index (with bounds check)
 *
 * Format: tn LIND <index/r/t>, <lower/r/t>, <upper/r/t>
 *
 * Assembly:
 *   BYn LIND (byte load index)                 Hex 0xFDC0+(n-1)
 *   Hn  LIND (halfword load index)             Hex 0xFD10+(n-1)
 *   Wn  LIND (word load index)                 Hex 0xAC+(n-1)
 *   Fn  LIND (floating load index)             Hex 0xFFC8+(n-1)
 *   Dn  LIND (double floating load index)      Hex 0xFFCC+(n-1)
 *
 * Operation:
 *   <index> -> Rn
 *   if <index> < <lower> or <index> > <upper> then
 *     1 -> K
 *     1 -> IX
 *   else
 *     0 -> K
 *     0 -> IX
 *   endif
 *
 * Description:
 *   The <index> operand is loaded into the specified register and checked
 *   to ensure it is within the bounds defined by <lower> and <upper> operands.
 *   If the index is out of bounds, the K flag and IX (Illegal Index) status
 *   bit are set. This instruction is used for array bounds checking.
 *
 * Trap conditions: Addressing traps
 *
 * Data status bits: K (out of bounds), IX trap bit (out of bounds)
 *
 * Reference: ND-500 Reference Manual, Chapter 15.8
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Lind.cs
 */
void nd500_instr_Lind(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 55-59) */
    if (fi->operand_count != 3) {
        printf("[ERROR] LIND at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Validate target register */
    if (fi->target_register < 1 || fi->target_register > 4) {
        printf("[ERROR] LIND at PC=0x%08X: Invalid target register %u\n",
               fi->address, fi->target_register);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read operands (like C# lines 62-64) */
    uint64_t index = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    uint64_t lower = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);
    uint64_t upper = nd500_read_operand_value(cpu, &fi->operands[2], fi->data_type);

    /* Load index into target register (like C# line 67) */
    nd500_write_integer_register(cpu, fi->target_register, (uint32_t)index);

    /* Data status bits per ND-500 Reference Manual, Chapter 15.8 ("LIND"):
     *   <index> = 0     -> Z
     *   <index>.signbit -> S
     * The bounds check drives K/IX; the LOADED index value also drives the
     * arithmetic Z and S flags (like a plain typed load). This was previously
     * omitted, making LIND the largest diverging family (~648 vectors) vs the
     * microword ND-5000, whose real B30 microcode (@001576 LIND ... ST,SAVA)
     * latches Z/S here. Mirrors the C# fix in Lind.cs (2026-07-23). */
    nd500_set_flags_zs(cpu, index, fi->data_type);

    /* For signed comparison, sign-extend based on data type */
    int64_t sindex = nd500_sign_extend_by_dtype(index, fi->data_type);
    int64_t slower = nd500_sign_extend_by_dtype(lower, fi->data_type);
    int64_t supper = nd500_sign_extend_by_dtype(upper, fi->data_type);

    /* Check if index is within bounds (lower <= index <= upper) (like C# line 70) */
    bool out_of_bounds = (sindex < slower) || (sindex > supper);

    /* Set status bits based on bounds check (like C# lines 73-74) */
    /* IX (Illegal Index) trap bit is at position 26 in ST1 */
    #define ND500_ST_IX (1u << 26)
    if (out_of_bounds) {
        cpu->ST1 |= ND500_FLAG_K;  /* K flag */
        cpu->ST1 |= ND500_ST_IX;   /* IX trap bit (bit 26 in ST1) */
    } else {
        cpu->ST1 &= ~ND500_FLAG_K;
        cpu->ST1 &= ~ND500_ST_IX;
    }
}
