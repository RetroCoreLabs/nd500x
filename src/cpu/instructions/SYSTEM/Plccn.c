/*
 * Plccn.c - ND-500 Plccn instruction (SYSTEM class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "instructions_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Plccn instruction - SYSTEM class, '87 extension
 *
 * PLCCN - Convert PLANC descriptor to ND-500 descriptor
 *
 * Format: W PLCCN <source/r/W>,<destination/w/W>
 *
 * Assembly:
 *   W PLCCN                                  Hex 0xFFFD (octal 177775)
 *
 * Operation (ND-05.009.4 section 15.15):
 *   (u-l+1) -> N
 *   a + 1 -> A
 *
 * The source operand addresses a PLANC descriptor in memory: three words a, l, u
 * in the order the manual draws them (address, lower, upper). The destination
 * addresses an ND-500 descriptor: two words N (number of elements) and A (address).
 *
 * What is settled (manual and the B30 microcode listing agree):
 *   N = u - l + 1, and 0 when that is negative.
 *   A = a + 4 * l. The manual prints "a + 1"; the microcode scales the lower
 *   bound by the element size, which is 4 for the W prefix, so "a + 1" is read
 *   as "a + l" in elements.
 *   Z = (N = 0), S = sign of the result, the other status bits are reset (6.5.1).
 *
 * What is NOT settled and is marked here on purpose:
 *   - The byte offsets inside the two descriptors (a, l, u at +0, +4, +8 and
 *     N, A at +0, +4) are taken from the order of the manual's drawing, not
 *     from a byte-level source.
 *   - Which value S takes (sign of N, or sign of A) and the other status bits.
 *   - Only the W prefix exists in the manual; the other element sizes of the
 *     microcode (PLCCNBI, BY, H, F, D, DD) have no opcode here.
 *
 * Trap conditions: addressing traps. The microcode needs both operands in
 * memory; a register or constant operand raises an illegal operand trap here.
 */
void nd500_instr_Plccn(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] PLCCN at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    const Nd500OperandDecoded* src = &fi->operands[0];
    const Nd500OperandDecoded* dst = &fi->operands[1];
    if (src->mode == ND500_ADDR_REGISTER || src->mode == ND500_ADDR_CONSTANT ||
        src->mode == ND500_ADDR_CONSTANT_SHORT ||
        dst->mode == ND500_ADDR_REGISTER || dst->mode == ND500_ADDR_CONSTANT ||
        dst->mode == ND500_ADDR_CONSTANT_SHORT) {
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t a = nd500_read_memory_32(cpu, src->effective_address);
    uint32_t l = nd500_read_memory_32(cpu, src->effective_address + 4u);
    uint32_t u = nd500_read_memory_32(cpu, src->effective_address + 8u);
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    int32_t n = (int32_t)(u - l + 1u);
    if (n < 0) {
        n = 0;
    }
    uint32_t addr = a + 4u * l;

    nd500_write_memory_32(cpu, dst->effective_address, (uint32_t)n);
    nd500_write_memory_32(cpu, dst->effective_address + 4u, addr);
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    nd500_set_flags_zs(cpu, (uint32_t)n, ND500_DTYPE_WORD);
    nd500_clear_flag(cpu, ND500_FLAG_O);
    nd500_clear_flag(cpu, ND500_FLAG_C);
}
