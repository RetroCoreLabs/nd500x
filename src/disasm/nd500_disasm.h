/*
 * nd500_disasm.h - disassembly formatting (presentation only; decoding is in the CPU)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#ifndef ND500_DISASM_H
#define ND500_DISASM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../cpu/cpu_protos.h"

/* Forward decl */
struct Nd500Machine;

/*
 * Format disassembly for [addr, addr+len) into out buffer.
 * Uses the core CPU decoder (nd500_decode_at) and does NOT implement any
 * fetch/decode/EA logic here. Purely presentation/formatting.
 * Returns number of bytes written to out (not including terminating NUL).
 */
size_t nd500_disasm_format_range(struct Nd500Machine* m,
                                 uint32_t addr,
                                 uint32_t len,
                                 char* out,
                                 size_t out_cap);

size_t nd500_disasm_format_range_json(struct Nd500Machine* m,
                                      uint32_t addr,
                                      uint32_t len,
                                      char* out,
                                      size_t out_cap);

/**
 * Authoritative operand formatter for disassembly output.
 * Handles all 15 ND-500 addressing modes with correct syntax.
 *
 * For REGISTER mode, shows correct register bank based on data type:
 * - BYTE/HALFWORD/WORD: W1-W4 (integer registers I1-I4)
 * - FLOAT: F1-F4 (float registers A1-A4)
 * - DOUBLEWORD: D1-D4 (double registers, A+E pairs)
 *
 * @param buf       Output buffer
 * @param cap       Buffer capacity
 * @param op        Decoded operand
 * @param dtype     Data type (determines register bank for REGISTER mode)
 * @param use_color Whether to include ANSI color codes (reserved for future use)
 * @return Number of characters written (excluding NUL terminator)
 */
int nd500_format_operand(char* buf, size_t cap,
                         const Nd500OperandDecoded* op,
                         Nd500DataType dtype,
                         bool use_color);

/**
 * Calculate branch target address from PC and displacement.
 * Shared function to avoid duplicate branch target calculation logic.
 *
 * @param pc           Current instruction address
 * @param displacement Signed displacement value from operand
 * @return Absolute target address
 */
uint32_t nd500_calc_branch_target(uint32_t pc, int32_t displacement);

/**
 * Extract displacement from operand data bytes (big-endian).
 *
 * @param op  Decoded operand
 * @return Signed displacement value
 */
int32_t nd500_get_operand_displacement(const struct Nd500OperandDecoded* op);

/**
 * Format instruction mnemonic with operands (no address/bytes).
 * Output format: "[dtype][reg] mnemonic operand1,operand2,..."
 * Example: "w1 :=           B.0x00" or "call         $134403143,$0"
 *
 * @param buf  Output buffer
 * @param cap  Buffer capacity
 * @param fi   Decoded instruction
 * @return Number of characters written (excluding NUL)
 */
size_t nd500_format_instruction(char* buf, size_t cap, const Nd500FetchedInstruction* fi);


#endif /* ND500_DISASM_H */
