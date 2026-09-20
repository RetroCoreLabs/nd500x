/*
 * instruction_helpers.h - helpers shared by the instruction handlers: memory access, flags, types
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#ifndef ND500_INSTRUCTION_HELPERS_H
#define ND500_INSTRUCTION_HELPERS_H

#include <stdint.h>
#include <stdbool.h>
#include "cpu_protos.h"

/*
 * Instruction Helper Functions
 *
 * This library provides common helper functions for ND-500 instruction
 * implementations, modeled after RetroCore's InstructionHelpers.cs.
 *
 * Categories:
 * - Memory access (multi-byte little-endian read/write)
 * - Operand access (abstract addressing mode handling)
 * - Flag manipulation (Z, S, C, O flag updates)
 * - Register access (integer, float, double registers)
 * - Arithmetic helpers (overflow detection, sign extension, masking)
 */

/* ============================================================================
 * MEMORY ACCESS HELPERS (Little-Endian Multi-Byte)
 * ============================================================================
 * ND-500 uses little-endian byte order in memory for multi-byte values.
 * These helpers abstract the bus layer and provide MMU-aware access.
 */

/**
 * Read 8-bit byte from virtual memory (DATA space)
 * @param cpu CPU state
 * @param vaddr Virtual address
 * @return 8-bit value
 */
uint8_t nd500_read_memory_8(Nd500Cpu* cpu, uint32_t vaddr);

/**
 * Read 8-bit byte from PROGRAM space (instruction fetch path)
 * On ND-500, program and data use separate capability tables (PMON vs DMON).
 * Used by CALL/CALLG to validate entry point opcodes.
 * Reference: ND-500 Reference Manual, Chapter 2 (Memory Architecture)
 * @param cpu CPU state
 * @param vaddr Virtual address
 * @return 8-bit value from program space
 */
uint8_t nd500_fetch_memory_8(Nd500Cpu* cpu, uint32_t vaddr);

/**
 * Write 8-bit byte to virtual memory
 * @param cpu CPU state
 * @param vaddr Virtual address
 * @param value 8-bit value to write
 */
void nd500_write_memory_8(Nd500Cpu* cpu, uint32_t vaddr, uint8_t value);

/**
 * Read 16-bit halfword from virtual memory (little-endian)
 * @param cpu CPU state
 * @param vaddr Virtual address
 * @return 16-bit value
 */
uint16_t nd500_read_memory_16(Nd500Cpu* cpu, uint32_t vaddr);

/**
 * Write 16-bit halfword to virtual memory (little-endian)
 * @param cpu CPU state
 * @param vaddr Virtual address
 * @param value 16-bit value to write
 */
void nd500_write_memory_16(Nd500Cpu* cpu, uint32_t vaddr, uint16_t value);

/**
 * Read 32-bit word from virtual memory (little-endian)
 * @param cpu CPU state
 * @param vaddr Virtual address
 * @return 32-bit value
 */
uint32_t nd500_read_memory_32(Nd500Cpu* cpu, uint32_t vaddr);

/**
 * Write 32-bit word to virtual memory (little-endian)
 * @param cpu CPU state
 * @param vaddr Virtual address
 * @param value 32-bit value to write
 */
void nd500_write_memory_32(Nd500Cpu* cpu, uint32_t vaddr, uint32_t value);

/**
 * Allocate a block from the buddy-system heap (ND-500 Reference Manual
 * section 3.3 / 15.13). Shared by GETB and ENTB.
 *
 * The heap variables are pointed to by TOS: +0 MAXL, +4 STAH, +8 ENDH,
 * +12+4*k FLOG[k] (freelist head for 2^k-word elements, 0 = empty). If the
 * exact-size freelist is empty, a larger element is split in halves with the
 * upper halves returned to their freelists.
 *
 * On success, stores the block's byte-address in *out_addr and returns 1.
 * On heap-not-initialized (TOS==0), log_size > MAXL, or no element of the
 * requested size or larger available, raises the STO trap (so the program's
 * own stack-overflow handler can seed/extend the heap - GETB/ENTB must never
 * touch STAH/ENDH themselves) and returns 0.
 *
 * @param cpu      CPU state
 * @param log_size log2 of the requested element size in words
 * @param pc       instruction address (for the trap)
 * @param out_addr receives the allocated block byte-address on success
 * @return 1 on success, 0 if STO was raised
 */
int nd500_heap_alloc_block(Nd500Cpu* cpu, uint8_t log_size, uint32_t pc,
                           uint32_t* out_addr);

/**
 * Read 64-bit doubleword from virtual memory (little-endian)
 * @param cpu CPU state
 * @param vaddr Virtual address
 * @return 64-bit value
 */
uint64_t nd500_read_memory_64(Nd500Cpu* cpu, uint32_t vaddr);

/**
 * Write 64-bit doubleword to virtual memory (little-endian)
 * @param cpu CPU state
 * @param vaddr Virtual address
 * @param value 64-bit value to write
 */
void nd500_write_memory_64(Nd500Cpu* cpu, uint32_t vaddr, uint64_t value);

/* ============================================================================
 * DOMAIN-AWARE MEMORY ACCESS (for ALT prefix support)
 * ============================================================================
 * The ALT prefix (0xC8) allows an instruction to access data in the
 * Current Alternative Domain (CAD) instead of the Current Executing Domain (CED).
 * This is used for cross-domain calls where the called routine needs to
 * read/write parameters in the caller's data space.
 *
 * Reference: ND-500 Reference Manual, Chapter 6 (Domain System)
 */

/**
 * Read 8-bit byte from virtual memory using specified domain
 * @param cpu CPU state
 * @param vaddr Virtual address
 * @param domain Domain for MMU translation (CED or CAD)
 * @return 8-bit value
 */
uint8_t nd500_read_memory_8_domain(Nd500Cpu* cpu, uint32_t vaddr, uint8_t domain);

/**
 * Write 8-bit byte to virtual memory using specified domain
 */
void nd500_write_memory_8_domain(Nd500Cpu* cpu, uint32_t vaddr, uint8_t value, uint8_t domain);

/**
 * Read 16-bit halfword from virtual memory using specified domain
 */
uint16_t nd500_read_memory_16_domain(Nd500Cpu* cpu, uint32_t vaddr, uint8_t domain);

/**
 * Write 16-bit halfword to virtual memory using specified domain
 */
void nd500_write_memory_16_domain(Nd500Cpu* cpu, uint32_t vaddr, uint16_t value, uint8_t domain);

/**
 * Read 32-bit word from virtual memory using specified domain
 */
uint32_t nd500_read_memory_32_domain(Nd500Cpu* cpu, uint32_t vaddr, uint8_t domain);

/**
 * Write 32-bit word to virtual memory using specified domain
 */
void nd500_write_memory_32_domain(Nd500Cpu* cpu, uint32_t vaddr, uint32_t value, uint8_t domain);

/**
 * Read 64-bit doubleword from virtual memory using specified domain
 */
uint64_t nd500_read_memory_64_domain(Nd500Cpu* cpu, uint32_t vaddr, uint8_t domain);

/**
 * Write 64-bit doubleword to virtual memory using specified domain
 */
void nd500_write_memory_64_domain(Nd500Cpu* cpu, uint32_t vaddr, uint64_t value, uint8_t domain);


/* ============================================================================
 * OPERAND ACCESS HELPERS
 * ============================================================================
 * Abstract addressing mode handling for reading/writing operand values.
 * These use the operand decode results from cpu_instr.c
 */

/**
 * Read byte (8-bit) from operand
 * Handles constants, registers, and memory operands
 * @param cpu CPU state
 * @param operand Decoded operand
 * @return 8-bit value
 */
uint8_t nd500_read_operand_byte(Nd500Cpu* cpu, const Nd500OperandDecoded* operand);

/**
 * Read halfword (16-bit) from operand
 * @param cpu CPU state
 * @param operand Decoded operand
 * @return 16-bit value
 */
uint16_t nd500_read_operand_halfword(Nd500Cpu* cpu, const Nd500OperandDecoded* operand);

/**
 * Read word (32-bit) from operand
 * @param cpu CPU state
 * @param operand Decoded operand
 * @return 32-bit value
 */
uint32_t nd500_read_operand_word(Nd500Cpu* cpu, const Nd500OperandDecoded* operand);

/**
 * Test whether a set of OTE bit changes is permitted by the Trap Enable
 * Modification Mask (TEMM). Per the ND-500 Reference Manual (sections 10.24
 * SETE / 10.25 CLTE / OTE load), a bit in OTE is modifiable only if the
 * corresponding TEMM bit is set; attempting to modify a non-modifiable bit
 * causes an illegal operand value trap.
 * @param changed_bits Bits that the operation would change (e.g. new ^ old, or 1<<n)
 * @param temm         The relevant TEMM half (TEMM1 for OTE1, TEMM2 for OTE2)
 * @return true if every changed bit is modifiable, false if any is protected
 */
bool nd500_temm_allows_change(uint32_t changed_bits, uint32_t temm);

/**
 * Read doubleword (64-bit) from operand
 * @param cpu CPU state
 * @param operand Decoded operand
 * @return 64-bit value
 */
uint64_t nd500_read_operand_doubleword(Nd500Cpu* cpu, const Nd500OperandDecoded* operand);

/**
 * Write word (32-bit) to operand
 * @param cpu CPU state
 * @param operand Decoded operand
 * @param value 32-bit value to write
 */
void nd500_write_operand_word(Nd500Cpu* cpu, const Nd500OperandDecoded* operand, uint32_t value);


/* ============================================================================
 * REGISTER ACCESS HELPERS
 * ============================================================================
 */

/**
 * Read integer register I1-I4
 * @param cpu CPU state
 * @param reg_num Register number (1-4)
 * @return 32-bit register value
 */
uint32_t nd500_read_integer_register(Nd500Cpu* cpu, uint8_t reg_num);

/**
 * Write integer register I1-I4
 * @param cpu CPU state
 * @param reg_num Register number (1-4)
 * @param value 32-bit value to write
 */
void nd500_write_integer_register(Nd500Cpu* cpu, uint8_t reg_num, uint32_t value);

/* The data-type part of register n: I for BI/BY/H/W (masked to the type),
 * A for F, E:A for D. */
uint64_t nd500_read_register_by_type(Nd500Cpu* cpu, uint8_t reg_num, Nd500DataType dtype);

/* INT and INTR (manual 10.34/10.35): the truncated or rounded integer part of
 * a float or double operand, in the same format, loaded into register n; Z and
 * S from the result. Exact on the ND-500 bits (no host float). */
void nd500_execute_integer_part(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi,
                                bool is_double, bool round);

/**
 * Read float register A1-A4 (single precision)
 * @param cpu CPU state
 * @param reg_num Register number (1-4)
 * @return 32-bit float value (raw bits)
 */
uint32_t nd500_read_float_register(Nd500Cpu* cpu, uint8_t reg_num);

/**
 * Write float register A1-A4 (single precision)
 * @param cpu CPU state
 * @param reg_num Register number (1-4)
 * @param value 32-bit float value (raw bits)
 */
void nd500_write_float_register(Nd500Cpu* cpu, uint8_t reg_num, uint32_t value);

/**
 * Read double register D1-D4 (A+E pair, 64-bit)
 * @param cpu CPU state
 * @param reg_num Register number (1-4)
 * @return 64-bit double value (raw bits)
 */
uint64_t nd500_read_double_register(Nd500Cpu* cpu, uint8_t reg_num);

/**
 * Write double register D1-D4 (A+E pair, 64-bit)
 * @param cpu CPU state
 * @param reg_num Register number (1-4)
 * @param value 64-bit double value (raw bits)
 */
void nd500_write_double_register(Nd500Cpu* cpu, uint8_t reg_num, uint64_t value);


/* ============================================================================
 * FLAG MANIPULATION HELPERS
 * ============================================================================
 * ND-500 status flags in ST1/ST2 registers:
 * Z (Zero), S (Sign), C (Carry), O (Overflow), K (Destination Full)
 * DZ (Divide by Zero), FU (Floating Underflow), FO (Floating Overflow)
 *
 * Flag bit positions in ST1 (low 32 bits of 64-bit status):
 * - Z: bit 5
 * - C: bit 6
 * - S: bit 7
 * - K: bit 8
 * - O: bit 9
 * - DZ: bit 12
 * - FU: bit 13
 * - FO: bit 14
 */

#define ND500_FLAG_PIA (1u << 1)   // Privileged Instructions Allowed
#define ND500_FLAG_PSD (1u << 4)   // Process Switch Disabled
#define ND500_FLAG_Z   (1u << 5)   // Zero flag
#define ND500_FLAG_C   (1u << 6)   // Carry flag
#define ND500_FLAG_S   (1u << 7)   // Sign flag
#define ND500_FLAG_K   (1u << 8)   // Destination full flag
#define ND500_FLAG_O   (1u << 9)   // Overflow flag
#define ND500_FLAG_IVO (1u << 11)  // Invalid operation flag
#define ND500_FLAG_DZ  (1u << 12)  // Divide by zero flag
#define ND500_FLAG_FU  (1u << 13)  // Floating underflow flag
#define ND500_FLAG_FO  (1u << 14)  // Floating overflow flag

// ND-500 Float constants (per Reference Manual 7.2.5)
// Format: sign(1) | exponent(9) | mantissa(22)
// Mantissa range: 0.5 <= M < 1.0 (implicit 0.1 binary prefix)
#define ND500_FLOAT_SIGN_MASK      0x80000000u           // Bit 31
#define ND500_FLOAT_EXPONENT_MASK  0x7FC00000u           // Bits 30-22 (9 bits)
#define ND500_FLOAT_MANTISSA_MASK  0x003FFFFFu           // Bits 21-0 (22 bits)
#define ND500_FLOAT_EXPONENT_BIAS  256
#define ND500_FLOAT_EXPONENT_SHIFT 22
#define ND500_FLOAT_MANTISSA_BITS  22

// ND-500 Double constants (per Reference Manual 7.2.6)
// Format: sign(1) | exponent(9) | mantissa(54)
// Mantissa range: 0.5 <= M < 1.0 (implicit 0.1 binary prefix)
#define ND500_DOUBLE_SIGN_MASK      0x8000000000000000ull  // Bit 63
#define ND500_DOUBLE_EXPONENT_MASK  0x7FC0000000000000ull  // Bits 62-54 (9 bits)
#define ND500_DOUBLE_MANTISSA_MASK  0x003FFFFFFFFFFFFFull  // Bits 53-0 (54 bits)
#define ND500_DOUBLE_EXPONENT_BIAS  256
#define ND500_DOUBLE_EXPONENT_SHIFT 54
#define ND500_DOUBLE_MANTISSA_BITS  54

/**
 * @brief Set Z and S from a result, and clear C and O.
 *
 * ND-05.009.4 6.5.1: "Bits that are set, reset or left unaffected are
 * mentioned explicitly. All data status bits not mentioned are reset."
 * The microcode's ST,SAVA status save does the same. A handler whose manual
 * list names C or O sets it after this call; K is not a data status bit and
 * is left alone.
 *
 * @param cpu    CPU state.
 * @param value  Result value.
 * @param dtype  Data type (determines sign bit position).
 */
void nd500_set_flags_zs(Nd500Cpu* cpu, uint64_t value, Nd500DataType dtype);

/**
 * @brief Set Z and S from a float/double result (-0.0 counts as zero), and
 *        clear C and O - the same manual rule as nd500_set_flags_zs().
 *
 * @param cpu        CPU state.
 * @param value      Float or double bits.
 * @param is_double  True for 64-bit double, false for 32-bit float.
 */
void nd500_set_flags_zs_float(Nd500Cpu* cpu, uint64_t value, bool is_double);

/*
 * Shared tail for floating-point arithmetic instructions.
 * Given the IEEE-754 result, sets Z,S from the result, clears C and O
 * (Reference rule 4040), sets FU/FO conditionally, and raises the FO/FU trap.
 * Returns the ND-500 result bits (float in low 32, double in 64) for the caller
 * to store to its destination operand or register.
 */
uint64_t nd500_float_finish(Nd500Cpu* cpu, uint32_t pc, double result, bool is_double);

/*
 * Status and traps for a floating point result from float_exact.h: Z and S
 * from the result bits, C and O cleared (manual 6.5.1), FO and FU set or
 * cleared from exc, and the FO or FU trap raised. Floating underflow sets Z
 * "in all cases" (manual 6.5.1, Z bit). The caller stores the result first,
 * because the manual stores the largest value or a signed zero on FO and FU.
 */
void nd500_float_status(Nd500Cpu* cpu, uint32_t pc, uint64_t bits, unsigned exc, bool is_double);

/*
 * Status for a float COMP or COMP2 from the exact difference r (float_exact.h).
 * COMPF @002143, COMPD @002147, COMP2F @002155 and COMP2D @002161 save the
 * AAP status with ST,SAVF (Z and S from the difference, C=0, O from floating
 * overflow in the engine's reading of SAVF) and then load the status ANDed
 * with NOT 0x6000, clearing FU and FO; so a compare leaves neither set and
 * raises no floating trap.
 */
void nd500_float_compare_status(Nd500Cpu* cpu, uint64_t r, unsigned exc, bool is_double);

/*
 * Store an F or D mathematical function result (float_math.h) in register Fn or Dn:
 * Z (from the bit pattern of the high word) and S from the result, C and O cleared (ST,SAVF in
 * FWRITE_AAP, ST,SAVA on the ALU exits), and the
 * invalid operation trap when exc has ND500_FX_IVO (the microcode's
 * IVOZRO/IVOMIN/IVOMAX exits all go through SET_IVO).
 */
void nd500_fm_store(Nd500Cpu* cpu, uint32_t pc, uint8_t reg_num, uint64_t r, unsigned exc, bool is_double);

/*
 * A mathematical function of one operand (SIN ... ALOG10): checks the
 * operand count and the register, reads the F or D operand, computes fn
 * (float_math.h) and stores the result with nd500_fm_store.
 */
typedef uint64_t (*Nd500FmUnary)(uint64_t x, bool is_double, unsigned* exc);
void nd500_fm_unary(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi, const char* name,
                    bool is_double, Nd500FmUnary fn);

/*
 * Register block and context block layout for SREGBL, LREGBL, SCNTXT and
 * LCNTXT (ND-05.009.4 16.27, whose mask table numbers the bits in octal; B30
 * STORERG_1 @011630 and LOADRG_1 @011366). Mask bit n selects one or two
 * words at fixed offsets (word number * 4) from the block address:
 *   bit 0..15   P L B R I1-I4 A1-A4 E1-E4   words 0..15
 *   bit 16      STS: ST1 and ST2            words 16, 17
 *   bit 17..23  PS TOS LL HL THA CED CAD    words 18..24
 *   bit 24      MIC                         words 27, 28
 *   bit 25..28  OTE CTE MTE TEMM            words 29-30, 31-32, 33-34, 35-36
 * The word offsets are the microcode's: each slot advances the address by the
 * mini-argument of its store word (STS writes two words, PS then skips one).
 */
#define ND500_REGBLOCK_MASK_BITS 29
int nd500_regblock_words(unsigned bit, int words[2]);            /* 0, 1 or 2 words */
uint32_t* nd500_regblock_register(Nd500Cpu* cpu, int word);      /* NULL: not emulated (MIC) */

/* A float (32-bit) or double (64-bit) operand as raw bits. */
uint64_t nd500_read_float_operand(Nd500Cpu* cpu, const Nd500OperandDecoded* operand, bool is_double);
void nd500_write_float_operand(Nd500Cpu* cpu, const Nd500OperandDecoded* operand, uint64_t bits, bool is_double);

/* Register n as F (An) or D (En:An), raw bits. */
uint64_t nd500_read_float_reg(Nd500Cpu* cpu, uint8_t reg_num, bool is_double);
void nd500_write_float_reg(Nd500Cpu* cpu, uint8_t reg_num, uint64_t bits, bool is_double);

/**
 * Set Z, S, and C flags
 * @param cpu CPU state
 * @param value Result value
 * @param dtype Data type
 * @param carry Carry flag value
 */
void nd500_set_flags_zsc(Nd500Cpu* cpu, uint64_t value, Nd500DataType dtype, bool carry);

/**
 * Set Z, S, C, and O flags
 * @param cpu CPU state
 * @param value Result value
 * @param dtype Data type
 * @param carry Carry flag value
 * @param overflow Overflow flag value
 */
void nd500_set_flags_zsco(Nd500Cpu* cpu, uint64_t value, Nd500DataType dtype, bool carry, bool overflow);

/**
 * Clear all status flags
 * @param cpu CPU state
 */
void nd500_clear_all_flags(Nd500Cpu* cpu);

/**
 * Set individual flag by bitmask
 * @param cpu CPU state
 * @param flag_mask Flag bitmask (e.g., ND500_FLAG_Z)
 */
void nd500_set_flag(Nd500Cpu* cpu, uint32_t flag_mask);

/**
 * Clear individual flag by bitmask
 * @param cpu CPU state
 * @param flag_mask Flag bitmask (e.g., ND500_FLAG_Z)
 */
void nd500_clear_flag(Nd500Cpu* cpu, uint32_t flag_mask);

/**
 * Test if flag is set
 * @param cpu CPU state
 * @param flag_mask Flag bitmask (e.g., ND500_FLAG_Z)
 * @return true if flag is set, false otherwise
 */
bool nd500_test_flag(Nd500Cpu* cpu, uint32_t flag_mask);


/* ============================================================================
 * ARITHMETIC HELPERS
 * ============================================================================
 */

/**
 * Sign-extend 6-bit CONSTANT_SHORT value to 32-bit
 * Bit 5 is the sign bit: values 0x20-0x3F represent -32 to -1
 * @param value 6-bit value (0x00-0x3F)
 * @return Sign-extended 32-bit value
 */
int32_t nd500_sign_extend_6bit(uint8_t value);

/**
 * Sign-extend byte to 32-bit
 * @param value 8-bit value
 * @return Sign-extended 32-bit value
 */
int32_t nd500_sign_extend_byte(uint8_t value);

/**
 * Sign-extend halfword to 32-bit
 * @param value 16-bit value
 * @return Sign-extended 32-bit value
 */
int32_t nd500_sign_extend_halfword(uint16_t value);

/**
 * Sign-extend word to 64-bit
 * @param value 32-bit value
 * @return Sign-extended 64-bit value
 */
int64_t nd500_sign_extend_word(uint32_t value);

/**
 * Mask value to byte (8-bit)
 * @param value Input value
 * @return Masked 8-bit value
 */
uint8_t nd500_mask_to_byte(uint64_t value);

/**
 * Mask value to halfword (16-bit)
 * @param value Input value
 * @return Masked 16-bit value
 */
uint16_t nd500_mask_to_halfword(uint64_t value);

/**
 * Mask value to word (32-bit)
 * @param value Input value
 * @return Masked 32-bit value
 */
uint32_t nd500_mask_to_word(uint64_t value);

/**
 * Detect addition overflow
 * @param a First operand
 * @param b Second operand
 * @param result Result of a + b
 * @param dtype Data type
 * @return true if overflow occurred, false otherwise
 */
bool nd500_detect_add_overflow(uint64_t a, uint64_t b, uint64_t result, Nd500DataType dtype);

/**
 * Detect subtraction overflow
 * @param a First operand (minuend)
 * @param b Second operand (subtrahend)
 * @param result Result of a - b
 * @param dtype Data type
 * @return true if overflow occurred, false otherwise
 */
bool nd500_detect_sub_overflow(uint64_t a, uint64_t b, uint64_t result, Nd500DataType dtype);

/**
 * Check if value is negative for given data type
 * @param value Value to check
 * @param dtype Data type (determines sign bit position)
 * @return true if negative (sign bit set), false otherwise
 */
bool nd500_is_negative(uint64_t value, Nd500DataType dtype);

/**
 * Sign-extend value based on data type
 * Helper to eliminate duplicated sign-extension logic in branch instructions
 * @param value Value to sign-extend
 * @param dtype Data type (BYTE, HALFWORD, or WORD)
 * @return Sign-extended 64-bit value
 */
int64_t nd500_sign_extend_by_dtype(uint64_t value, Nd500DataType dtype);

/**
 * Detect carry for addition based on data type
 * Helper to eliminate duplicated carry detection in arithmetic instructions
 * @param result Result of addition (before masking)
 * @param dtype Data type
 * @return true if carry occurred
 */
bool nd500_detect_carry_add(uint64_t result, Nd500DataType dtype);

/* ============================================================================
 * C# COMPATIBILITY HELPERS (match RetroCore InstructionHelpers.cs)
 * ============================================================================
 */

/**
 * Mask value to data type (clear upper bits - like C# MaskToDataType)
 * @param value Value to mask
 * @param dtype Data type
 * @return Masked value (BI=0x01, BY=0xFF, H=0xFFFF, W/D=unchanged)
 */
uint32_t nd500_mask_to_datatype(uint64_t value, Nd500DataType dtype);

/**
 * Read operand value (unified reader - like C# ReadOperandValue)
 * Handles constants, registers, and memory operands
 * @param cpu CPU state
 * @param op Decoded operand
 * @param dtype Data type to read
 * @return Value read
 */
uint64_t nd500_read_operand_value(Nd500Cpu* cpu, const Nd500OperandDecoded* op, Nd500DataType dtype);

/**
 * Write operand value (unified writer - like C# WriteOperandValue)
 * Writes to memory operands (constants not writable)
 * @param cpu CPU state
 * @param op Decoded operand
 * @param value Value to write
 * @param dtype Data type to write
 */
void nd500_write_operand_value(Nd500Cpu* cpu, const Nd500OperandDecoded* op, uint64_t value, Nd500DataType dtype);

/* ============================================================================
 * VALIDATION HELPERS (reduce code duplication)
 * ============================================================================
 */

/**
 * Instruction name enum for validation helpers (type-safe, no string duplication)
 * Add new instructions here as they are implemented
 */
typedef enum {
    INSTR_UNKNOWN = 0,
    INSTR_ADD,
    INSTR_SUBTRACT,
    INSTR_MULTIPLY,
    INSTR_DIVIDE,
    INSTR_UDIV,
    INSTR_CLR,
    INSTR_NEG,
    INSTR_REM,
    INSTR_AXI,
    INSTR_MUL4,
    /* Add more as needed */
} Nd500InstrName;

/**
 * Get instruction name string from enum (for error messages only)
 * @param instr Instruction enum
 * @return Instruction name string (never NULL)
 */
const char* nd500_instr_name_str(Nd500InstrName instr);

/**
 * Validate operand count for instruction
 * @param cpu CPU state
 * @param fi Fetched instruction
 * @param expected_count Expected number of operands
 * @param instr Instruction name enum
 * @return true if valid, false if error (trap triggered)
 */
bool nd500_validate_operand_count(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi,
                                   uint8_t expected_count, Nd500InstrName instr);

/**
 * Validate target register for register-implicit instructions
 * @param cpu CPU state
 * @param fi Fetched instruction
 * @param instr Instruction name enum
 * @return true if valid (1-4), false if error (trap triggered)
 */
bool nd500_validate_target_register(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi,
                                     Nd500InstrName instr);

/* ============================================================================
 * STACK OPERATION HELPERS (for CALL/RETURN instructions)
 * ============================================================================
 * ND-500 stack grows upward (toward higher addresses).
 * L register points to current stack top.
 */

/**
 * Push 32-bit word onto stack
 * L -> memory, L += 4
 * @param cpu CPU state
 * @param value 32-bit value to push
 */
void nd500_push_word(Nd500Cpu* cpu, uint32_t value);

/**
 * Pop 32-bit word from stack
 * L -= 4, memory -> return value
 * @param cpu CPU state
 * @return 32-bit value popped
 */
uint32_t nd500_pop_word(Nd500Cpu* cpu);

/**
 * Push 64-bit doubleword onto stack
 * L -> memory (8 bytes), L += 8
 * @param cpu CPU state
 * @param value 64-bit value to push
 */
void nd500_push_doubleword(Nd500Cpu* cpu, uint64_t value);

/**
 * Pop 64-bit doubleword from stack
 * L -= 8, memory -> return value
 * @param cpu CPU state
 * @return 64-bit value popped
 */
uint64_t nd500_pop_doubleword(Nd500Cpu* cpu);


/* ============================================================================
 * BITFIELD OPERATION HELPERS (for BITFIELD instructions)
 * ============================================================================
 */

/**
 * Extract bitfield from value
 * @param value Source value
 * @param offset Bit offset (0-31 for word)
 * @param width Bit width (1-32)
 * @return Extracted bitfield (right-aligned, zero-extended)
 */
uint32_t nd500_extract_bitfield(uint32_t value, uint8_t offset, uint8_t width);

/**
 * Insert bitfield into destination
 * @param dest Destination value
 * @param src Source bitfield (right-aligned)
 * @param offset Bit offset (0-31 for word)
 * @param width Bit width (1-32)
 * @return Destination with bitfield inserted
 */
uint32_t nd500_insert_bitfield(uint32_t dest, uint32_t src, uint8_t offset, uint8_t width);

/**
 * Set bit at memory address
 * @param cpu CPU state
 * @param addr Memory address (byte-addressed)
 * @param bit_offset Bit offset within byte (0-7)
 */
void nd500_set_bit(Nd500Cpu* cpu, uint32_t addr, uint8_t bit_offset);

/**
 * Clear bit at memory address
 * @param cpu CPU state
 * @param addr Memory address (byte-addressed)
 * @param bit_offset Bit offset within byte (0-7)
 */
void nd500_clear_bit(Nd500Cpu* cpu, uint32_t addr, uint8_t bit_offset);

/**
 * Test bit at memory address
 * @param cpu CPU state
 * @param addr Memory address (byte-addressed)
 * @param bit_offset Bit offset within byte (0-7)
 * @return true if bit is set, false otherwise
 */
bool nd500_test_bit(Nd500Cpu* cpu, uint32_t addr, uint8_t bit_offset);


/* ============================================================================
 * LOOP OPERATION HELPERS (for BRANCH Loop* instructions)
 * ============================================================================
 */

/**
 * Decrement counter and test for loop continuation
 * counter--, return (counter != 0)
 * @param counter Pointer to counter variable
 * @return true if loop should continue (counter != 0 after decrement)
 */
bool nd500_loop_decrement_and_test(uint32_t* counter);


/* ============================================================================
 * PACKED ARITHMETIC HELPERS (for ARITHMETIC P* instructions)
 * ============================================================================
 * Packed operations perform SIMD-style operations on multiple sub-elements.
 * Element type determines how the word is partitioned:
 * - BYTE: 4 bytes per word (4x8-bit operations)
 * - HALFWORD: 2 halfwords per word (2x16-bit operations)
 */

/**
 * Packed add - add corresponding elements
 * @param a First packed operand
 * @param b Second packed operand
 * @param element_type Element data type (BYTE or HALFWORD)
 * @return Packed result
 */
uint32_t nd500_packed_add(uint32_t a, uint32_t b, Nd500DataType element_type);

/**
 * Packed subtract - subtract corresponding elements
 * @param a First packed operand (minuend)
 * @param b Second packed operand (subtrahend)
 * @param element_type Element data type (BYTE or HALFWORD)
 * @return Packed result
 */
uint32_t nd500_packed_sub(uint32_t a, uint32_t b, Nd500DataType element_type);

/**
 * Packed multiply - multiply corresponding elements
 * @param a First packed operand
 * @param b Second packed operand
 * @param element_type Element data type (BYTE or HALFWORD)
 * @return Packed result (lower part of each product)
 */
uint32_t nd500_packed_mul(uint32_t a, uint32_t b, Nd500DataType element_type);

/* ============================================================================
 * STATUS REGISTER AND PRIVILEGE CHECKING
 * ============================================================================
 * ND-500 Status Register (ST) is a 64-bit register split into ST1 (low 32)
 * and ST2 (high 32). Contains flags, trap status bits, and control bits.
 *
 * Key Status Bits (Reference: ND-500 Reference Manual Chapter 6.5):
 *   Bit 0: Reserved
 *   Bit 1: PIA (Privileged Instructions Allowed) - privilege mode flag
 *   Bit 2: PD (Part Done)
 *   Bit 3: M (Monitor Call)
 *   Bit 4: T (Test)
 *   Bit 5: Z (Zero)
 *   Bit 6: C (Carry)
 *   Bit 7: S (Sign)
 *   Bit 8: K (Flag - signaling/sync)
 *   Bit 9: O (Overflow)
 *   Bit 10: Reserved
 *   Bits 11-63: Trap status bits (IVO, DZ, FU, FO, BO, IOV, SIT, etc.)
 */

/**
 * Check if CPU is in privileged mode (PIA bit set in status register)
 * Privileged mode allows execution of system instructions (PMON, DMON, etc.)
 *
 * @param cpu CPU state
 * @return true if privileged mode, false if user mode
 */
bool nd500_is_privileged(Nd500Cpu* cpu);

/**
 * Get status register bit value
 * Reads a specific bit from the 64-bit status register (ST1 + ST2)
 *
 * @param cpu CPU state
 * @param bit_number Bit number (0-63, 0-31 from ST1, 32-63 from ST2)
 * @return true if bit is set, false otherwise
 */
bool nd500_get_status_bit(Nd500Cpu* cpu, uint8_t bit_number);

/**
 * Set status register bit
 * Sets a specific bit in the 64-bit status register (ST1 + ST2)
 *
 * @param cpu CPU state
 * @param bit_number Bit number (0-63, 0-31 from ST1, 32-63 from ST2)
 */
void nd500_set_status_bit(Nd500Cpu* cpu, uint8_t bit_number);

/**
 * Clear status register bit
 * Clears a specific bit in the 64-bit status register (ST1 + ST2)
 *
 * @param cpu CPU state
 * @param bit_number Bit number (0-63, 0-31 from ST1, 32-63 from ST2)
 */
void nd500_clear_status_bit(Nd500Cpu* cpu, uint8_t bit_number);

/**
 * Check privilege and trap if not privileged
 * Helper for privileged instructions - checks PIA bit and raises IIC trap if not set
 *
 * @param cpu CPU state
 * @param pc PC where instruction is executing
 * @return true if privileged (safe to continue), false if trapped
 */
bool nd500_require_privilege(Nd500Cpu* cpu, uint32_t pc);

/* Status Register Bit Definitions */
#define ND500_ST_BIT_PIA  1   /* Privileged Instructions Allowed (bit 1) */
#define ND500_ST_BIT_PD   2   /* Part Done (bit 2) */
#define ND500_ST_BIT_M    3   /* Monitor Call (bit 3) */
#define ND500_ST_BIT_T    4   /* Test (bit 4) */
#define ND500_ST_BIT_Z    5   /* Zero (bit 5) */
#define ND500_ST_BIT_C    6   /* Carry (bit 6) */
#define ND500_ST_BIT_S    7   /* Sign (bit 7) */
#define ND500_ST_BIT_K    8   /* K Flag (bit 8) */
#define ND500_ST_BIT_O    9   /* Overflow (bit 9) */

/* ============================================================================
 * STRING DESCRIPTOR SUPPORT
 * ============================================================================
 * ND-500 string descriptors for BCD/ASCII operations (based on StringDescriptor.cs)
 */

/**
 * String descriptor structure (8 bytes in memory)
 *
 * Format:
 * - Bytes 0-3: Element count (N) with embedded flags for BCD/ASCII
 * - Bytes 4-7: Base address (A)
 *
 * For BCD/ASCII operations, element count word contains:
 * - Bits 26-24: Sign representation (3 bits)
 * - Bits 23-18: Scaling factor (6 bits, signed -32..+31)
 * - Bits 17-13: Field width (5 bits, 0..31)
 * - Bits 12-0: Actual element count (13 bits)
 */
typedef struct {
    uint32_t element_count;              // Number of elements
    uint32_t base_address;               // Address of element 0
} Nd500StringDescriptor;

/**
 * Load string descriptor from memory
 * @param cpu CPU state
 * @param desc_addr Address where 8-byte descriptor is stored
 * @param desc Output descriptor structure
 * @return true if successful, false on error
 */
bool nd500_load_string_descriptor(Nd500Cpu* cpu, uint32_t desc_addr, Nd500StringDescriptor* desc);

/**
 * Read element value from string (generic)
 * @param cpu CPU state
 * @param desc String descriptor
 * @param index Element index
 * @param dtype Data type for element
 * @return Element value
 */
uint64_t nd500_string_read_element(Nd500Cpu* cpu, const Nd500StringDescriptor* desc,
                                    uint32_t index, Nd500DataType dtype);

/**
 * Write element value to string (based on WriteElementValue)
 * Handles different data types (byte, halfword, word, etc.)
 * @param cpu CPU state
 * @param desc String descriptor
 * @param index Element index
 * @param value Value to write
 * @param dtype Data type for element
 */
void nd500_string_write_element(Nd500Cpu* cpu, const Nd500StringDescriptor* desc,
                                uint32_t index, uint64_t value, Nd500DataType dtype);

/**
 * Get element size in bytes for data type
 * @param dtype Data type
 * @return Size in bytes (1, 2, 4, or 8)
 */
uint32_t nd500_get_element_size(Nd500DataType dtype);

/* ============================================================================
 * BCD (Binary Coded Decimal) SUPPORT
 * ============================================================================
 * BCD value reading/writing for PCOMP and arithmetic operations
 */

/**
 * Clear string operation flags (S, C, O)
 * Common helper to reduce code duplication in string instructions.
 * @param cpu CPU state
 */
void nd500_string_clear_unused_flags(Nd500Cpu* cpu);

/* ============================================================================
 * FLOATING-POINT CONVERSION (ND-500 <-> IEEE 754 <-> Integer)
 * ============================================================================
 *
 * ND-500 Float Format (32-bit):
 *   - 1 bit sign (bit 31)
 *   - 9 bits exponent (bits 30-22), bias 256
 *   - 22+1 bits mantissa (bits 21-0), implicit leading 1
 *   - Exponent = 0 means exactly zero (no denormalized numbers)
 *   - M range: 0.5 <= M < 1.0
 *
 * ND-500 Double Format (64-bit):
 *   - 1 bit sign (bit 63)
 *   - 9 bits exponent (bits 62-54), bias 256
 *   - 54+1 bits mantissa (bits 53-0), implicit leading 1
 *   - Exponent = 0 means exactly zero (no denormalized numbers)
 *   - M range: 0.5 <= M < 1.0
 *
 * PRECISION TRADE-OFF:
 * ====================
 * Transcendental functions (sin, cos, sqrt, log, etc.) are implemented by:
 *   1. Converting ND-500 format to IEEE 754
 *   2. Using standard C math library (works on IEEE 754)
 *   3. Converting result back to ND-500 format
 *
 * This introduces a small precision loss for double-precision:
 *   - ND-500 double: 55-bit mantissa (~16.5 decimal digits)
 *   - IEEE 754 double: 53-bit mantissa (~15.9 decimal digits)
 *   - Loss: ~2 bits (~0.6 decimal digits)
 *
 * For single-precision, IEEE 754 has MORE precision (24 vs 23 bits),
 * so no loss occurs.
 *
 * This trade-off is acceptable for most applications. For bit-exact
 * reproduction of original ND-500 hardware results, consider:
 *   - Using Berkeley SoftFloat library (http://www.jhauser.us/arithmetic/SoftFloat.html)
 *   - Implementing native ND-500 arithmetic routines
 *
 * IMPORTANT: Never use BitConverter or direct bit reinterpretation to
 * convert between ND-500 and IEEE 754 formats. The formats are different:
 *   - Different exponent bias (ND-500: 256, IEEE single: 127, IEEE double: 1023)
 *   - Different mantissa interpretation (ND-500: 0.5-1.0, IEEE: 1.0-2.0)
 * Always use the conversion functions below.
 */

// Single precision conversions
uint32_t nd500_float_from_int32(int32_t value);
int32_t nd500_float_to_int32(uint32_t nd500_bits);
float nd500_float_to_ieee754(uint32_t nd500_bits);
uint32_t nd500_float_from_ieee754(float ieee_value);
bool nd500_float_is_zero(uint32_t nd500_bits);
bool nd500_float_is_negative(uint32_t nd500_bits);

// Double precision conversions
uint64_t nd500_double_from_int64(int64_t value);
int64_t nd500_double_to_int64(uint64_t nd500_bits);
double nd500_double_to_ieee754(uint64_t nd500_bits);
uint64_t nd500_double_from_ieee754(double ieee_value);
bool nd500_double_is_zero(uint64_t nd500_bits);
bool nd500_double_is_negative(uint64_t nd500_bits);

// Precision conversion
uint32_t nd500_double_to_single(uint64_t nd500_double_bits);
uint64_t nd500_single_to_double(uint32_t nd500_float_bits);

// Full-range ND-500 NATIVE float/double <-> host-double codec.
//
// Unlike nd500_float_to_ieee754/_from_ieee754 (which manipulate IEEE bit-fields and
// therefore clamp the value to the IEEE single/double sub-range and disagree on
// denormal inputs), these carry the value in a host DOUBLE, which spans the full
// native bias-256 range (+/-8.6e-78 .. +/-5.8e76). Overflow/underflow are reported by
// native EXPONENT range per Reference Manual sections 2.5.1.4 / 2.5.3.6 and the FU/FO
// trap definitions (a signed exponent requiring more than 9 bits): FO when the native
// exponent field would exceed 511, FU when it would fall below 1 (result stored as zero,
// keeping the sign). Used by the float-arithmetic cluster + LOOP + SET1 helpers below.
double   nd500_native_single_to_double(uint32_t nd500_bits);
uint32_t nd500_native_single_from_double(double value, bool* out_overflow, bool* out_underflow);
double   nd500_native_double_to_double(uint64_t nd500_bits);
uint64_t nd500_native_double_from_double(double value, bool* out_overflow, bool* out_underflow);

// IEEE-754 operand helpers
double nd500_read_operand_as_ieee_float(Nd500Cpu* cpu, const Nd500OperandDecoded* operand, bool is_double);
void nd500_write_operand_from_ieee_float(Nd500Cpu* cpu, const Nd500OperandDecoded* operand, double value, bool is_double);

/**
 * Integer divide by zero, as the B30 microcode leaves it. Returns the quotient the
 * instruction must store and sets the status: Z S C O reset, DZ set, then S for a
 * negative dividend or Z for a zero one. The caller stores the quotient (and a zero
 * remainder where it has one) and then raises the trap.
 *
 * Signed (DIV_INT, used by / DIV2 DIV3 DIV4): ST,SAVA @024130 resets the data status;
 * a positive dividend gives the largest positive value of the data type (@024133-
 * @024135, OR BM14 = DZ), a negative one the most negative value (INTDN @024136-@024140,
 * OR SARG 010200 = DZ + S), a zero one zero (INTDZRO @024141-@024143, OR SARG 010040 =
 * DZ + Z). The manual says the same in 6.5.1: "the largest possible value in the
 * destination with the sign of the dividend ... Zero divided by zero gives a result
 * of zero."
 * Unsigned (UDIV_COMM @027502-@027505): all ones, or zero for a zero dividend, with
 * ST,SAVA on that value and then OR BM14.
 */
uint32_t nd500_int_divide_by_zero(Nd500Cpu* cpu, uint32_t dividend, uint8_t data_type, bool is_unsigned);

#endif /* ND500_INSTRUCTION_HELPERS_H */
