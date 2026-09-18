/*
 * nd500_instructions.h - instruction dispatch table declarations
 * Pre-generated and committed as source file
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#ifndef ND500_INSTRUCTIONS_H
#define ND500_INSTRUCTIONS_H
#include <stdint.h>

typedef struct {
    uint16_t opcode;
    const char* mnemonic;
    uint8_t operands;
    uint8_t prefixes_mask;
    uint8_t variant;
    uint8_t has_variable_operands;  /* 1 if instruction accepts variable operands (CALL, CALLG, POLY) */
    uint32_t op_templates[4];
} Nd500Instr;
extern const Nd500Instr g_nd500_instrs[];
extern const unsigned g_nd500_instrs_count;

/* Forward declaration for CPU types */
typedef struct Nd500Cpu Nd500Cpu;
typedef struct Nd500FetchedInstruction Nd500FetchedInstruction;

/* Instruction execution function pointer type */
typedef void (*InstrExecFunc)(Nd500Cpu*, const Nd500FetchedInstruction*);

/* Dispatch table: 65536 entries indexed by opcode (sparse, mostly NULL) */
extern InstrExecFunc g_instr_exec_table[65536];

#endif /* ND500_INSTRUCTIONS_H */
