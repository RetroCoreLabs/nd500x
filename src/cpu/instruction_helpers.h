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
 * Set Z and S flags for float/double values (handles -0.0 as zero)
 * @param cpu CPU state
 * @param value Float or double bits (raw IEEE-754 representation)
 * @param is_double True for 64-bit double, false for 32-bit float
 */
void nd500_set_flags_zs_float(Nd500Cpu* cpu, uint64_t value, bool is_double);

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
 * Sign representation for BCD/ASCII values
 */
typedef enum {
    ND500_SIGN_TRAILING_SEPARATE = 0,  // Sign in separate trailing byte
    ND500_SIGN_LEADING_SEPARATE = 1,   // Sign in separate leading byte
    ND500_SIGN_TRAILING_OVERPUNCH = 2, // Sign overpunched in trailing digit
    ND500_SIGN_LEADING_OVERPUNCH = 3,  // Sign overpunched in leading digit
    ND500_SIGN_UNSIGNED = 4,            // Unsigned value
    ND500_SIGN_RESERVED_5 = 5,          // Reserved
    ND500_SIGN_RESERVED_6 = 6,          // Reserved
    ND500_SIGN_RESERVED_7 = 7           // Reserved
} Nd500SignRepresentation;

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
    Nd500SignRepresentation sign_repr;   // Sign representation (BCD/ASCII only)
    int8_t scaling_factor;               // Scaling factor -32..+31 (BCD/ASCII only)
    uint8_t field_width;                 // Field width in nibbles/bytes (BCD/ASCII only)
    bool is_bcd_packed;                  // True if BCD packed descriptor
    bool is_ascii;                       // True if ASCII descriptor
} Nd500StringDescriptor;

/**
 * Load string descriptor from memory
 * @param cpu CPU state
 * @param desc_addr Address where 8-byte descriptor is stored
 * @param is_bcd_packed True for BCD packed operations
 * @param is_ascii True for ASCII operations
 * @param desc Output descriptor structure
 * @return true if successful, false on error
 */
bool nd500_load_string_descriptor(Nd500Cpu* cpu, uint32_t desc_addr,
                                   bool is_bcd_packed, bool is_ascii,
                                   Nd500StringDescriptor* desc);

/**
 * Get element address from descriptor
 * @param desc String descriptor
 * @param index Element index
 * @return Element address, or 0 if index out of range
 */
uint32_t nd500_string_get_element_address(const Nd500StringDescriptor* desc, uint32_t index);

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
 * Validate string descriptor
 * @param desc String descriptor
 * @return true if valid, false otherwise
 */
bool nd500_string_descriptor_is_valid(const Nd500StringDescriptor* desc);

/* ============================================================================
 * BCD (Binary Coded Decimal) SUPPORT
 * ============================================================================
 * BCD value reading/writing for PCOMP and arithmetic operations
 */

/**
 * Read packed BCD value from memory using descriptor
 * @param cpu CPU state
 * @param desc String descriptor (must be BCD packed)
 * @return BCD value as int64_t (sign-extended)
 */
int64_t nd500_read_packed_bcd_value(Nd500Cpu* cpu, const Nd500StringDescriptor* desc);

/**
 * Write packed BCD value to memory using descriptor
 * @param cpu CPU state
 * @param desc String descriptor (must be BCD packed)
 * @param value Value to write
 */
void nd500_write_packed_bcd_value(Nd500Cpu* cpu, const Nd500StringDescriptor* desc, int64_t value);

/**
 * Write packed BCD value to memory with rounding
 * Used by PADDR, PSUBR, PMPYR, PPACKR, PUPACKR instructions.
 * Applies rounding when destination scaling factor causes precision loss.
 * @param cpu CPU state
 * @param desc String descriptor (must be BCD packed)
 * @param value Value to write (unscaled integer representation)
 * @param source_scale Scaling factor of source value
 */
void nd500_write_packed_bcd_value_rounded(Nd500Cpu* cpu, const Nd500StringDescriptor* desc,
                                          int64_t value, int8_t source_scale);

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

// IEEE-754 operand helpers
double nd500_read_operand_as_ieee_float(Nd500Cpu* cpu, const Nd500OperandDecoded* operand, bool is_double);
void nd500_write_operand_from_ieee_float(Nd500Cpu* cpu, const Nd500OperandDecoded* operand, double value, bool is_double);

#endif /* ND500_INSTRUCTION_HELPERS_H */
