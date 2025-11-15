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
 * Read 8-bit byte from virtual memory
 * @param cpu CPU state
 * @param vaddr Virtual address
 * @return 8-bit value
 */
uint8_t nd500_read_memory_8(Nd500Cpu* cpu, uint32_t vaddr);

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
 * Read doubleword (64-bit) from operand
 * @param cpu CPU state
 * @param operand Decoded operand
 * @return 64-bit value
 */
uint64_t nd500_read_operand_doubleword(Nd500Cpu* cpu, const Nd500OperandDecoded* operand);

/**
 * Write byte (8-bit) to operand
 * Handles register and memory operands
 * @param cpu CPU state
 * @param operand Decoded operand
 * @param value 8-bit value to write
 */
void nd500_write_operand_byte(Nd500Cpu* cpu, const Nd500OperandDecoded* operand, uint8_t value);

/**
 * Write halfword (16-bit) to operand
 * @param cpu CPU state
 * @param operand Decoded operand
 * @param value 16-bit value to write
 */
void nd500_write_operand_halfword(Nd500Cpu* cpu, const Nd500OperandDecoded* operand, uint16_t value);

/**
 * Write word (32-bit) to operand
 * @param cpu CPU state
 * @param operand Decoded operand
 * @param value 32-bit value to write
 */
void nd500_write_operand_word(Nd500Cpu* cpu, const Nd500OperandDecoded* operand, uint32_t value);

/**
 * Write doubleword (64-bit) to operand
 * @param cpu CPU state
 * @param operand Decoded operand
 * @param value 64-bit value to write
 */
void nd500_write_operand_doubleword(Nd500Cpu* cpu, const Nd500OperandDecoded* operand, uint64_t value);


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

#define ND500_FLAG_Z   (1u << 5)   // Zero flag
#define ND500_FLAG_C   (1u << 6)   // Carry flag
#define ND500_FLAG_S   (1u << 7)   // Sign flag
#define ND500_FLAG_K   (1u << 8)   // Destination full flag
#define ND500_FLAG_O   (1u << 9)   // Overflow flag
#define ND500_FLAG_DZ  (1u << 12)  // Divide by zero flag
#define ND500_FLAG_FU  (1u << 13)  // Floating underflow flag
#define ND500_FLAG_FO  (1u << 14)  // Floating overflow flag

/**
 * Set Z and S flags based on result value
 * @param cpu CPU state
 * @param value Result value
 * @param dtype Data type (determines sign bit position)
 */
void nd500_set_flags_zs(Nd500Cpu* cpu, uint64_t value, Nd500DataType dtype);

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

/**
 * Check if instruction uses float registers and print stub message if so
 * @param fi Fetched instruction
 * @param instr Instruction name enum
 * @return true if float/double (not implemented), false if integer (can continue)
 */
bool nd500_check_float_stub(const Nd500FetchedInstruction* fi, Nd500InstrName instr);

/* ============================================================================
 * STACK OPERATION HELPERS (for CALL/RETURN instructions)
 * ============================================================================
 * ND-500 stack grows upward (toward higher addresses).
 * L register points to current stack top.
 */

/**
 * Push 32-bit word onto stack
 * L → memory, L += 4
 * @param cpu CPU state
 * @param value 32-bit value to push
 */
void nd500_push_word(Nd500Cpu* cpu, uint32_t value);

/**
 * Pop 32-bit word from stack
 * L -= 4, memory → return value
 * @param cpu CPU state
 * @return 32-bit value popped
 */
uint32_t nd500_pop_word(Nd500Cpu* cpu);

/**
 * Push 64-bit doubleword onto stack
 * L → memory (8 bytes), L += 8
 * @param cpu CPU state
 * @param value 64-bit value to push
 */
void nd500_push_doubleword(Nd500Cpu* cpu, uint64_t value);

/**
 * Pop 64-bit doubleword from stack
 * L -= 8, memory → return value
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

#endif /* ND500_INSTRUCTION_HELPERS_H */
