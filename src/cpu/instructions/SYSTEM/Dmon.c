/*
 * Dmon.c - ND-500 DMON instruction (SYSTEM class)
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
 * DMON instruction - SYSTEM class
 *
 * Mnemonic: dmon
 * Operands: 0
 * Opcode: 0xFF16 (177426 octal)
 *
 * Operation: Turn on data memory management system
 *
 * Description:
 * Privileged instruction.
 * Following data accesses will be mapped on a physical segment through the
 * memory management system, rather than being interpreted directly as physical addresses.
 * If the data memory management system is already turned on, the instruction has no effect.
 *
 * Trap conditions: Illegal instruction code (IIC) if not privileged
 * Data status bits: Unaffected
 *
 * Reference: ND-500 Reference Manual ND-05.009.4 EN, Page 301, Section 16.13
 */
void nd500_instr_Dmon(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    (void)fi;  /* Unused parameter */

    /* Check privilege - DMON requires supervisor mode */
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* Trapped - not privileged */
    }

    /* Enable data MMU (for data accesses only, not instruction fetches) */
    nd500_mmu_enable_data(cpu);

    /* PC will be advanced automatically by cpu_step() */
}
