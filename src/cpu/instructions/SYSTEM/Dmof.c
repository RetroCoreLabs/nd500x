/*
 * Dmof.c - ND-500 DMOF instruction (SYSTEM class)
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
 * DMOF instruction - SYSTEM class
 *
 * Mnemonic: dmof
 * Operands: 0
 * Opcode: 0xFF18 (177430 octal)
 *
 * Operation: Turn off data memory management system
 *
 * Description:
 * Privileged instruction.
 * Following data accesses will be interpreted directly as physical addresses,
 * rather than being mapped on a physical segment through the memory management system.
 * If the memory management system is already turned off, the instruction has no effect.
 *
 * Trap conditions: Illegal instruction code (IIC) if not privileged
 * Data status bits: Unaffected
 *
 * Reference: ND-500 Reference Manual ND-05.009.4 EN, Page 303, Section 16.15
 */
void nd500_instr_Dmof(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    (void)fi;  /* Unused parameter */

    /* Check privilege - DMOF requires supervisor mode */
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* Trapped - not privileged */
    }

    /* Disable data MMU (data accesses will now use physical addresses) */
    nd500_mmu_disable_data(cpu);

    /* PC will be advanced automatically by cpu_step() */
}
