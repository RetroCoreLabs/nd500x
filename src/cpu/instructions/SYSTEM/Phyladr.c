/*
 * Phyladr.c - ND-500 Phyladr instruction (SYSTEM class)
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
#include "nd500_mmu.h"
#include <stdio.h>

/**
 * Phyladr instruction - SYSTEM class
 *
 * PHYLADR - Get Physical Address
 *
 * Format: tn PHYLADR <operand/aa/W>
 *
 * Assembly:
 *   tn PHYLADR (get physical address)        Hex 0xFFF0+(n-1)
 *
 * Operation: tr(addr(<operand>)) -> In
 *
 * Description:
 *   The physical address corresponding to the logical address of the
 *   operand is loaded into the specified integer register. This instruction
 *   performs address translation from logical to physical address space.
 *   This is an '87 extension instruction for advanced memory management.
 *
 * Trap conditions: Addressing traps
 *
 * Data status bits: the manual names none. The B30 microcode (PHYLADR
 *   @001026 -> @004573) saves the status with ST,SAVA on the result: Z and S
 *   from the physical address, C and O reset. A register operand goes to
 *   ILL_OP_SPEC (the SAVC1 test at PHLADR_1 @004561): illegal operand
 *   specifier.
 *
 * Reference: ND-500 Reference Manual, Chapter 16.37
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Phyladr.cs
 */
void nd500_instr_Phyladr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 41-45) */
    if (fi->operand_count != 1) {
        printf("[ERROR] PHYLADR at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Validate target register */
    if (fi->target_register < 1 || fi->target_register > 4) {
        printf("[ERROR] PHYLADR at PC=0x%08X: Invalid target register %u\n",
               fi->address, fi->target_register);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* A register or constant has no address (PHLADR_1 @004561 -> ILL_OP_SPEC). */
    const Nd500OperandDecoded* op = &fi->operands[0];
    if (op->mode == ND500_ADDR_REGISTER || op->mode == ND500_ADDR_CONSTANT ||
        op->mode == ND500_ADDR_CONSTANT_SHORT) {
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get logical (virtual) address of operand (like C# line 48) */
    uint32_t logical_address = op->effective_address;

    /* Translate virtual to physical address using MMU (like C# line 52) */
    /* nd500_mmu_translate handles both MMU-enabled and MMU-disabled cases */
    uint32_t physical_address;
    if (cpu->machine && cpu->machine->mmu_enabled) {
        physical_address = nd500_mmu_translate(cpu, logical_address, 0, 0);
        /* Check for trap during translation */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
    } else {
        /* No MMU - physical = logical */
        physical_address = logical_address;
    }

    /* Load physical address into target register (like C# line 55) */
    nd500_write_integer_register(cpu, fi->target_register, physical_address);

    /* ST,SAVA @004573: Z and S from the result, C and O reset. This set only
     * Z before, following the C# port. */
    nd500_set_flags_zs(cpu, physical_address, ND500_DTYPE_WORD);
}
