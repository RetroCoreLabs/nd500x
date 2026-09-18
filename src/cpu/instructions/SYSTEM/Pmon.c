/*
 * Pmon.c - ND-500 PMON instruction (SYSTEM class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "machine_protos.h"
#include "nd500_mmu.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * PMON instruction - SYSTEM class
 *
 * Mnemonic: pmon
 * Operands: 0
 * Opcode: 0xFF17 (177427 octal)
 *
 * Operation: Turn on program memory management system; L -> P
 *
 * Description:
 * Privileged instruction.
 * Following instruction accesses will be mapped on a physical segment through the
 * memory management system, rather than being interpreted directly as physical addresses.
 * The virtual address of the next instruction to be executed is found in the L register.
 * If the program memory management system is already turned on, control is transferred
 * to the instruction pointed to by the L register and the instruction has no further effect.
 *
 * Trap conditions: Illegal instruction code (IIC) if not privileged
 * Data status bits: Unaffected
 *
 * Reference: ND-500 Reference Manual ND-05.009.4 EN, Page 302, Section 16.14
 */
void nd500_instr_Pmon(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    (void)fi;  /* Unused parameter */

    /* Check privilege - PMON requires supervisor mode */
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* Trapped - not privileged */
    }

    /* Enable program MMU (for instruction fetches only, not data accesses) */
    nd500_mmu_enable_program(cpu);

    /* Transfer control to virtual address in L register */
    /* The next instruction fetch will be translated through the program MMU */
    cpu->PC = cpu->L;
}
