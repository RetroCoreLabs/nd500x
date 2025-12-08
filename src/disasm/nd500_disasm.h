#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Forward decl to avoid heavy deps */
struct Nd500Machine;
struct Nd500OperandDecoded;

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
 * @param buf       Output buffer
 * @param cap       Buffer capacity
 * @param op        Decoded operand
 * @param use_color Whether to include ANSI color codes (reserved for future use)
 * @return Number of characters written (excluding NUL terminator)
 */
int nd500_format_operand(char* buf, size_t cap, 
                         const struct Nd500OperandDecoded* op,
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


