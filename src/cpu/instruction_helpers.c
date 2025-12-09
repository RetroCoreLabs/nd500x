#include "instruction_helpers.h"
#include "cpu_protos.h"
#include "nd500_mmu.h"
#include "../machine/machine_protos.h"
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * INTERNAL HELPER: MMU-Aware Memory Access via bus functions
 * ============================================================================
 * We use nd500_bus_read/write functions from machine/io.c which handle MMU
 * translation automatically when cpu->machine->mmu_enabled is set.
 */


/* ============================================================================
 * MEMORY ACCESS HELPERS (Little-Endian Multi-Byte)
 * ============================================================================
 */

uint8_t nd500_read_memory_8(Nd500Cpu* cpu, uint32_t vaddr) {
    if (!cpu || !cpu->machine) return 0;

    // Translate virtual to physical address if MMU is enabled
    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate(cpu, vaddr, 0, 0); // is_write=0, is_instruction=0
        if (nd500_trap_occurred()) return 0;  // Trap occurred during translation
    }

    return nd500_bus_read8(cpu->machine, paddr);
}

void nd500_write_memory_8(Nd500Cpu* cpu, uint32_t vaddr, uint8_t value) {
    if (!cpu || !cpu->machine) return;

    // Translate virtual to physical address if MMU is enabled
    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate(cpu, vaddr, 1, 0); // is_write=1, is_instruction=0
        if (nd500_trap_occurred()) return;  // Trap occurred during translation
    }

    nd500_bus_write8(cpu->machine, paddr, value);
}

uint16_t nd500_read_memory_16(Nd500Cpu* cpu, uint32_t vaddr) {
    if (!cpu || !cpu->machine) return 0;

    // Translate virtual to physical address if MMU is enabled
    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate(cpu, vaddr, 0, 0); // is_write=0, is_instruction=0
        if (nd500_trap_occurred()) return 0;  // Trap occurred during translation
    }

    // Read two bytes BIG-ENDIAN from physical address (ND-500 spec)
    uint8_t b0 = nd500_bus_read8(cpu->machine, paddr);
    uint8_t b1 = nd500_bus_read8(cpu->machine, paddr + 1);
    return ((uint16_t)b0 << 8) | (uint16_t)b1;
}

void nd500_write_memory_16(Nd500Cpu* cpu, uint32_t vaddr, uint16_t value) {
    if (!cpu || !cpu->machine) return;

    // Translate virtual to physical address if MMU is enabled
    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate(cpu, vaddr, 1, 0); // is_write=1, is_instruction=0
        if (nd500_trap_occurred()) return;  // Trap occurred during translation
    }

    // Write two bytes BIG-ENDIAN to physical address (ND-500 spec)
    nd500_bus_write8(cpu->machine, paddr,     (uint8_t)((value >> 8) & 0xFF));
    nd500_bus_write8(cpu->machine, paddr + 1, (uint8_t)(value & 0xFF));
}

uint32_t nd500_read_memory_32(Nd500Cpu* cpu, uint32_t vaddr) {
    if (!cpu || !cpu->machine) return 0;

    // Translate virtual to physical address if MMU is enabled
    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate(cpu, vaddr, 0, 0); // is_write=0, is_instruction=0
        if (nd500_trap_occurred()) return 0;  // Trap occurred during translation
    }

    // Read four bytes BIG-ENDIAN from physical address (ND-500 spec)
    uint8_t b0 = nd500_bus_read8(cpu->machine, paddr);
    uint8_t b1 = nd500_bus_read8(cpu->machine, paddr + 1);
    uint8_t b2 = nd500_bus_read8(cpu->machine, paddr + 2);
    uint8_t b3 = nd500_bus_read8(cpu->machine, paddr + 3);
    return ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16) |
           ((uint32_t)b2 << 8) | (uint32_t)b3;
}

void nd500_write_memory_32(Nd500Cpu* cpu, uint32_t vaddr, uint32_t value) {
    if (!cpu || !cpu->machine) return;

    // Translate virtual to physical address if MMU is enabled
    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate(cpu, vaddr, 1, 0); // is_write=1, is_instruction=0
        if (nd500_trap_occurred()) {
            TRACE("[TRACE] write_memory_32: MMU trap! vaddr=0x%08X value=0x%08X\n", vaddr, value);
            return;  // Trap occurred during translation
        }
    }

    // Debug: warn if writing to address beyond physical memory
    if (paddr >= cpu->machine->memory_size) {
        TRACE("[TRACE] write_memory_32: BEYOND MEMORY! vaddr=0x%08X paddr=0x%08X value=0x%08X mem_size=0x%X\n",
              vaddr, paddr, value, cpu->machine->memory_size);
        return;  // Don't write beyond memory!
    }

    TRACE("[TRACE] write_memory_32: vaddr=0x%08X paddr=0x%08X value=0x%08X\n", vaddr, paddr, value);

    // Write four bytes BIG-ENDIAN to physical address (ND-500 spec)
    nd500_bus_write8(cpu->machine, paddr,     (uint8_t)((value >> 24) & 0xFF));
    nd500_bus_write8(cpu->machine, paddr + 1, (uint8_t)((value >> 16) & 0xFF));
    nd500_bus_write8(cpu->machine, paddr + 2, (uint8_t)((value >> 8) & 0xFF));
    nd500_bus_write8(cpu->machine, paddr + 3, (uint8_t)(value & 0xFF));
}

uint64_t nd500_read_memory_64(Nd500Cpu* cpu, uint32_t vaddr) {
    // Read eight bytes BIG-ENDIAN (ND-500 spec)
    uint32_t high = nd500_read_memory_32(cpu, vaddr);
    uint32_t low  = nd500_read_memory_32(cpu, vaddr + 4);
    return ((uint64_t)high << 32) | (uint64_t)low;
}

void nd500_write_memory_64(Nd500Cpu* cpu, uint32_t vaddr, uint64_t value) {
    // Write eight bytes BIG-ENDIAN (ND-500 spec)
    nd500_write_memory_32(cpu, vaddr,     (uint32_t)((value >> 32) & 0xFFFFFFFF));
    nd500_write_memory_32(cpu, vaddr + 4, (uint32_t)(value & 0xFFFFFFFF));
}


/* ============================================================================
 * OPERAND ACCESS HELPERS
 * ============================================================================
 */

uint8_t nd500_read_operand_byte(Nd500Cpu* cpu, const Nd500OperandDecoded* operand) {
    return (uint8_t)nd500_read_operand_value(cpu, operand, ND500_DTYPE_BYTE);
}

uint16_t nd500_read_operand_halfword(Nd500Cpu* cpu, const Nd500OperandDecoded* operand) {
    return (uint16_t)nd500_read_operand_value(cpu, operand, ND500_DTYPE_HALFWORD);
}

uint32_t nd500_read_operand_word(Nd500Cpu* cpu, const Nd500OperandDecoded* operand) {
    return (uint32_t)nd500_read_operand_value(cpu, operand, ND500_DTYPE_WORD);
}

uint64_t nd500_read_operand_doubleword(Nd500Cpu* cpu, const Nd500OperandDecoded* operand) {
    return nd500_read_operand_value(cpu, operand, ND500_DTYPE_DOUBLEWORD);
}

void nd500_write_operand_word(Nd500Cpu* cpu, const Nd500OperandDecoded* operand, uint32_t value) {
    // Handle register direct
    if (operand->mode == ND500_ADDR_REGISTER) {
        nd500_write_integer_register(cpu, operand->reg, value);
        return;
    }

    // Memory operand
    nd500_write_memory_32(cpu, operand->effective_address, value);
}


/* ============================================================================
 * REGISTER ACCESS HELPERS
 * ============================================================================
 */

uint32_t nd500_read_integer_register(Nd500Cpu* cpu, uint8_t reg_num) {
    if (reg_num < 1 || reg_num > 4) {
        fprintf(stderr, "ND-500: Invalid integer register number %u\n", reg_num);
        return 0;
    }
    return cpu->I[reg_num - 1];  // I1=I[0], I2=I[1], I3=I[2], I4=I[3]
}

void nd500_write_integer_register(Nd500Cpu* cpu, uint8_t reg_num, uint32_t value) {
    if (reg_num < 1 || reg_num > 4) {
        fprintf(stderr, "ND-500: Invalid integer register number %u\n", reg_num);
        return;
    }
    cpu->I[reg_num - 1] = value;
}

uint32_t nd500_read_float_register(Nd500Cpu* cpu, uint8_t reg_num) {
    if (reg_num < 1 || reg_num > 4) {
        fprintf(stderr, "ND-500: Invalid float register number %u\n", reg_num);
        return 0;
    }
    return cpu->A[reg_num - 1];  // A1=A[0], A2=A[1], A3=A[2], A4=A[3]
}

void nd500_write_float_register(Nd500Cpu* cpu, uint8_t reg_num, uint32_t value) {
    if (reg_num < 1 || reg_num > 4) {
        fprintf(stderr, "ND-500: Invalid float register number %u\n", reg_num);
        return;
    }
    cpu->A[reg_num - 1] = value;
}

uint64_t nd500_read_double_register(Nd500Cpu* cpu, uint8_t reg_num) {
    if (reg_num < 1 || reg_num > 4) {
        fprintf(stderr, "ND-500: Invalid double register number %u\n", reg_num);
        return 0;
    }
    // D1 = A1:E1 (A is low 32 bits, E is high 32 bits)
    uint32_t a_val = cpu->A[reg_num - 1];
    uint32_t e_val = cpu->E[reg_num - 1];
    return (uint64_t)a_val | ((uint64_t)e_val << 32);
}

void nd500_write_double_register(Nd500Cpu* cpu, uint8_t reg_num, uint64_t value) {
    if (reg_num < 1 || reg_num > 4) {
        fprintf(stderr, "ND-500: Invalid double register number %u\n", reg_num);
        return;
    }
    // Split 64-bit value into A and E registers
    cpu->A[reg_num - 1] = (uint32_t)(value & 0xFFFFFFFF);
    cpu->E[reg_num - 1] = (uint32_t)((value >> 32) & 0xFFFFFFFF);
}


/* ============================================================================
 * FLAG MANIPULATION HELPERS
 * ============================================================================
 */

void nd500_set_flags_zs(Nd500Cpu* cpu, uint64_t value, Nd500DataType dtype) {
    // Clear Z and S flags first
    cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_S);

    // Set Z flag if value is zero (masked to data type)
    uint64_t mask = 0;
    bool sign_bit = false;

    switch (dtype) {
        case ND500_DTYPE_BYTE:
            mask = 0xFF;
            sign_bit = (value & 0x80) != 0;
            break;
        case ND500_DTYPE_HALFWORD:
            mask = 0xFFFF;
            sign_bit = (value & 0x8000) != 0;
            break;
        case ND500_DTYPE_WORD:
            mask = 0xFFFFFFFF;
            sign_bit = (value & 0x80000000) != 0;
            break;
        case ND500_DTYPE_DOUBLEWORD:
            mask = 0xFFFFFFFFFFFFFFFFULL;
            sign_bit = (value & 0x8000000000000000ULL) != 0;
            break;
    }

    if ((value & mask) == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    }

    // Set S flag if sign bit is set
    if (sign_bit) {
        cpu->ST1 |= ND500_FLAG_S;
    }
}

void nd500_set_flags_zsc(Nd500Cpu* cpu, uint64_t value, Nd500DataType dtype, bool carry) {
    // Set Z and S flags
    nd500_set_flags_zs(cpu, value, dtype);

    // Set or clear C flag
    if (carry) {
        cpu->ST1 |= ND500_FLAG_C;
    } else {
        cpu->ST1 &= ~ND500_FLAG_C;
    }
}

void nd500_set_flags_zsco(Nd500Cpu* cpu, uint64_t value, Nd500DataType dtype, bool carry, bool overflow) {
    // Set Z, S, and C flags
    nd500_set_flags_zsc(cpu, value, dtype, carry);

    // Set or clear O flag
    if (overflow) {
        cpu->ST1 |= ND500_FLAG_O;
    } else {
        cpu->ST1 &= ~ND500_FLAG_O;
    }
}

void nd500_clear_all_flags(Nd500Cpu* cpu) {
    cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_C | ND500_FLAG_S |
                  ND500_FLAG_K | ND500_FLAG_O);
}

void nd500_set_flag(Nd500Cpu* cpu, uint32_t flag_mask) {
    cpu->ST1 |= flag_mask;
}

void nd500_clear_flag(Nd500Cpu* cpu, uint32_t flag_mask) {
    cpu->ST1 &= ~flag_mask;
}

bool nd500_test_flag(Nd500Cpu* cpu, uint32_t flag_mask) {
    return (cpu->ST1 & flag_mask) != 0;
}


/* ============================================================================
 * ARITHMETIC HELPERS
 * ============================================================================
 */

int32_t nd500_sign_extend_6bit(uint8_t value) {
    // Sign extend 6-bit to 32-bit
    // Bit 5 is the sign bit (values 0x20-0x3F are negative: -32 to -1)
    if (value & 0x20) {
        // Negative: extend with 1s
        return (int32_t)(value | 0xFFFFFFC0);
    }
    return (int32_t)(value & 0x3F);
}

int32_t nd500_sign_extend_byte(uint8_t value) {
    // Sign extend 8-bit to 32-bit
    return (int32_t)((int8_t)value);
}

int32_t nd500_sign_extend_halfword(uint16_t value) {
    // Sign extend 16-bit to 32-bit
    return (int32_t)((int16_t)value);
}

int64_t nd500_sign_extend_word(uint32_t value) {
    // Sign extend 32-bit to 64-bit
    return (int64_t)((int32_t)value);
}

uint8_t nd500_mask_to_byte(uint64_t value) {
    return (uint8_t)(value & 0xFF);
}

uint16_t nd500_mask_to_halfword(uint64_t value) {
    return (uint16_t)(value & 0xFFFF);
}

uint32_t nd500_mask_to_word(uint64_t value) {
    return (uint32_t)(value & 0xFFFFFFFF);
}

bool nd500_detect_add_overflow(uint64_t a, uint64_t b, uint64_t result, Nd500DataType dtype) {
    // Overflow occurs when two operands with the same sign produce a result with different sign
    // Check sign bits based on data type

    bool a_sign, b_sign, r_sign;

    switch (dtype) {
        case ND500_DTYPE_BYTE:
            a_sign = (a & 0x80) != 0;
            b_sign = (b & 0x80) != 0;
            r_sign = (result & 0x80) != 0;
            break;
        case ND500_DTYPE_HALFWORD:
            a_sign = (a & 0x8000) != 0;
            b_sign = (b & 0x8000) != 0;
            r_sign = (result & 0x8000) != 0;
            break;
        case ND500_DTYPE_WORD:
            a_sign = (a & 0x80000000) != 0;
            b_sign = (b & 0x80000000) != 0;
            r_sign = (result & 0x80000000) != 0;
            break;
        case ND500_DTYPE_DOUBLEWORD:
            a_sign = (a & 0x8000000000000000ULL) != 0;
            b_sign = (b & 0x8000000000000000ULL) != 0;
            r_sign = (result & 0x8000000000000000ULL) != 0;
            break;
        default:
            return false;
    }

    // Overflow if both operands have same sign but result has different sign
    return (a_sign == b_sign) && (a_sign != r_sign);
}

bool nd500_detect_sub_overflow(uint64_t a, uint64_t b, uint64_t result, Nd500DataType dtype) {
    // Overflow occurs when subtracting operands with different signs produces result with wrong sign
    // result = a - b

    bool a_sign, b_sign, r_sign;

    switch (dtype) {
        case ND500_DTYPE_BYTE:
            a_sign = (a & 0x80) != 0;
            b_sign = (b & 0x80) != 0;
            r_sign = (result & 0x80) != 0;
            break;
        case ND500_DTYPE_HALFWORD:
            a_sign = (a & 0x8000) != 0;
            b_sign = (b & 0x8000) != 0;
            r_sign = (result & 0x8000) != 0;
            break;
        case ND500_DTYPE_WORD:
            a_sign = (a & 0x80000000) != 0;
            b_sign = (b & 0x80000000) != 0;
            r_sign = (result & 0x80000000) != 0;
            break;
        case ND500_DTYPE_DOUBLEWORD:
            a_sign = (a & 0x8000000000000000ULL) != 0;
            b_sign = (b & 0x8000000000000000ULL) != 0;
            r_sign = (result & 0x8000000000000000ULL) != 0;
            break;
        default:
            return false;
    }

    // Overflow if operands have different signs and result has same sign as subtrahend
    return (a_sign != b_sign) && (b_sign == r_sign);
}

bool nd500_is_negative(uint64_t value, Nd500DataType dtype) {
    switch (dtype) {
        case ND500_DTYPE_BYTE:
            return (value & 0x80) != 0;
        case ND500_DTYPE_HALFWORD:
            return (value & 0x8000) != 0;
        case ND500_DTYPE_WORD:
            return (value & 0x80000000) != 0;
        case ND500_DTYPE_DOUBLEWORD:
            return (value & 0x8000000000000000ULL) != 0;
        default:
            return false;
    }
}

int64_t nd500_sign_extend_by_dtype(uint64_t value, Nd500DataType dtype) {
    switch (dtype) {
        case ND500_DTYPE_BYTE:
            return nd500_sign_extend_byte((uint8_t)value);
        case ND500_DTYPE_HALFWORD:
            return nd500_sign_extend_halfword((uint16_t)value);
        case ND500_DTYPE_WORD:
            return nd500_sign_extend_word((uint32_t)value);
        case ND500_DTYPE_DOUBLEWORD:
            /* Already 64-bit, just reinterpret as signed */
            return (int64_t)value;
        default:
            return (int64_t)value;
    }
}

bool nd500_detect_carry_add(uint64_t result, Nd500DataType dtype) {
    switch (dtype) {
        case ND500_DTYPE_BYTE:
            return (result > 0xFF);
        case ND500_DTYPE_HALFWORD:
            return (result > 0xFFFF);
        case ND500_DTYPE_WORD:
            return (result > 0xFFFFFFFF);
        case ND500_DTYPE_DOUBLEWORD:
            /* For 64-bit, overflow cannot be detected this way - need different method */
            return false;  /* TODO: Implement properly if needed */
        default:
            return false;
    }
}

/* ============================================================================
 * C# COMPATIBILITY HELPERS (match RetroCore InstructionHelpers.cs)
 * ============================================================================
 */

uint32_t nd500_mask_to_datatype(uint64_t value, Nd500DataType dtype) {
    switch (dtype) {
        case ND500_DTYPE_BYTE:
            return (uint32_t)(value & 0xFF);
        case ND500_DTYPE_HALFWORD:
            return (uint32_t)(value & 0xFFFF);
        case ND500_DTYPE_WORD:
        case ND500_DTYPE_DOUBLEWORD:
            return (uint32_t)value;  /* W, F, D - no masking needed for 32-bit */
        default:
            return (uint32_t)value;
    }
}

uint64_t nd500_read_operand_value(Nd500Cpu* cpu, const Nd500OperandDecoded* op, Nd500DataType dtype) {
    /* Handle CONSTANT_SHORT - value embedded in address code (low 6 bits)
     * The 6-bit value is SIGNED: bit 5 is sign bit
     * Values 0x00-0x1F = 0 to 31 (positive)
     * Values 0x20-0x3F = -32 to -1 (negative, sign-extended) */
    if (op->mode == ND500_ADDR_CONSTANT_SHORT) {
        uint8_t raw_value = op->address_code & 0x3F;
        int32_t signed_value = nd500_sign_extend_6bit(raw_value);
        return (uint64_t)(uint32_t)signed_value;
    }

    /* Handle CONSTANT - value in data array (BIG-ENDIAN per ND-500 spec)
     * 
     * Per ND-500 Reference Manual Table 13 "Treatment of constants as operands":
     * Constants must be SIGN-EXTENDED (SX) when constant size < operation type size.
     * 
     * | Instruction type | :S (6-bit) | :B (byte) | :H (half) | :W (word) |
     * |------------------|------------|-----------|-----------|-----------|
     * | BY               | SX         | NC        | IOS       | IOS       |
     * | H                | SX         | SX        | NC        | IOS       |
     * | W                | SX         | SX        | SX        | NC        |
     * 
     * SX = sign extended, NC = no conversion, IOS = illegal operand specifier
     * 
     * Note: "SX - sign extended (unless instruction calls for unsigned)"
     * Even for unsigned operations (UMUL, UDIV), constants are first sign-extended,
     * then the result is treated as unsigned by the instruction.
     */
    if (op->mode == ND500_ADDR_CONSTANT) {
        /* First, read the raw value (big-endian) */
        uint64_t raw_val = 0;
        switch (op->data_len) {
            case 1:
                raw_val = op->data[0];
                break;
            case 2:
                raw_val = (uint16_t)((op->data[0] << 8) | op->data[1]);
                break;
            case 4:
                raw_val = (uint32_t)((op->data[0] << 24) | (op->data[1] << 16) |
                                    (op->data[2] << 8) | op->data[3]);
                break;
            case 8:
                raw_val = ((uint64_t)op->data[0] << 56) | ((uint64_t)op->data[1] << 48) |
                          ((uint64_t)op->data[2] << 40) | ((uint64_t)op->data[3] << 32) |
                          ((uint64_t)op->data[4] << 24) | ((uint64_t)op->data[5] << 16) |
                          ((uint64_t)op->data[6] << 8) | (uint64_t)op->data[7];
                break;
            default:
                return 0;
        }
        
        /* Apply sign extension per Table 13 when constant size < dtype size */
        switch (dtype) {
            case ND500_DTYPE_DOUBLEWORD:
                /* D operations: byte/half/word constants get 32LZ (zero fill), not SX */
                /* Per Table 13: D with :W or :F = 32LZ (32 least significant bits zero filled) */
                /* For simplicity, just return raw value - D operations are rare with small constants */
                return raw_val;
                
            case ND500_DTYPE_WORD:
                /* W operations: SX for byte and halfword constants */
                if (op->data_len == 1) {
                    /* Sign-extend byte to word */
                    return (uint64_t)(uint32_t)(int32_t)(int8_t)raw_val;
                }
                if (op->data_len == 2) {
                    /* Sign-extend halfword to word */
                    return (uint64_t)(uint32_t)(int32_t)(int16_t)raw_val;
                }
                return raw_val;  /* NC for word constant */
                
            case ND500_DTYPE_HALFWORD:
                /* H operations: SX for byte constants */
                if (op->data_len == 1) {
                    /* Sign-extend byte to halfword */
                    return (uint64_t)(uint16_t)(int16_t)(int8_t)raw_val;
                }
                return raw_val;  /* NC for halfword constant */
                
            case ND500_DTYPE_BYTE:
                /* BY operations: NC for byte constant */
                return raw_val;
                
            default:
                return raw_val;
        }
    }

    /* Handle registers - dispatch to correct register bank based on data type
     * Per ND-500 Reference Manual:
     * - Integer types (BI, BY, H, W) use I1-I4 registers
     * - Float type (F) uses A1-A4 registers
     * - Double type (D) uses D1-D4 registers (A+E pairs)
     */
    if (op->mode == ND500_ADDR_REGISTER) {
        switch (dtype) {
            case ND500_DTYPE_FLOAT:
                return nd500_read_float_register(cpu, op->reg);
            case ND500_DTYPE_DOUBLEWORD:
                return nd500_read_double_register(cpu, op->reg);
            default:
                /* BYTE, HALFWORD, WORD -> integer registers */
                return nd500_read_integer_register(cpu, op->reg);
        }
    }

    /* Handle memory operands */
    switch (dtype) {
        case ND500_DTYPE_BYTE:
            return nd500_read_memory_8(cpu, op->effective_address);
        case ND500_DTYPE_HALFWORD:
            return nd500_read_memory_16(cpu, op->effective_address);
        case ND500_DTYPE_WORD:
        case ND500_DTYPE_FLOAT:
            return nd500_read_memory_32(cpu, op->effective_address);
        case ND500_DTYPE_DOUBLEWORD:
            return nd500_read_memory_64(cpu, op->effective_address);
        default:
            return 0;
    }
}

void nd500_write_operand_value(Nd500Cpu* cpu, const Nd500OperandDecoded* op, uint64_t value, Nd500DataType dtype) {
    /* Constants are not writable */
    if (op->mode == ND500_ADDR_CONSTANT || op->mode == ND500_ADDR_CONSTANT_SHORT) {
        return;
    }

    /* Handle registers - dispatch to correct register bank based on data type
     * Per ND-500 Reference Manual (page 901):
     * - I1-I4: "32-bit general registers for word and partial word operations"
     * - A1-A4: "32-bit floating-point accumulators for real number arithmetic"
     * - D1-D4 (A+E): "64-bit floating point accumulators for double precision"
     *
     * Also: "When using the integer registers for BIt, BYte and Halfword, the
     * unused upper part of the register is always zero-filled rather than
     * sign-extended when data is loaded to the register." */
    if (op->mode == ND500_ADDR_REGISTER) {
        switch (dtype) {
            case ND500_DTYPE_FLOAT:
                /* Float -> A1-A4 registers */
                nd500_write_float_register(cpu, op->reg, (uint32_t)value);
                return;
            case ND500_DTYPE_DOUBLEWORD:
                /* Double -> D1-D4 registers (A+E pairs) */
                nd500_write_double_register(cpu, op->reg, value);
                return;
            case ND500_DTYPE_BYTE:
                /* Byte -> I registers, zero-fill upper bits */
                nd500_write_integer_register(cpu, op->reg, (uint32_t)(value & 0xFF));
                return;
            case ND500_DTYPE_HALFWORD:
                /* Halfword -> I registers, zero-fill upper bits */
                nd500_write_integer_register(cpu, op->reg, (uint32_t)(value & 0xFFFF));
                return;
            default:
                /* Word -> I registers */
                nd500_write_integer_register(cpu, op->reg, (uint32_t)value);
                return;
        }
    }

    /* Handle memory operands */
    switch (dtype) {
        case ND500_DTYPE_BYTE:
            nd500_write_memory_8(cpu, op->effective_address, (uint8_t)value);
            break;
        case ND500_DTYPE_HALFWORD:
            nd500_write_memory_16(cpu, op->effective_address, (uint16_t)value);
            break;
        case ND500_DTYPE_WORD:
        case ND500_DTYPE_FLOAT:
            nd500_write_memory_32(cpu, op->effective_address, (uint32_t)value);
            break;
        case ND500_DTYPE_DOUBLEWORD:
            nd500_write_memory_64(cpu, op->effective_address, value);
            break;
        default:
            break;
    }
}


/* ============================================================================
 * STACK OPERATION HELPERS
 * ============================================================================
 */

void nd500_push_word(Nd500Cpu* cpu, uint32_t value) {
    nd500_write_memory_32(cpu, cpu->L, value);
    cpu->L += 4;
}

uint32_t nd500_pop_word(Nd500Cpu* cpu) {
    cpu->L -= 4;
    return nd500_read_memory_32(cpu, cpu->L);
}

void nd500_push_doubleword(Nd500Cpu* cpu, uint64_t value) {
    nd500_write_memory_64(cpu, cpu->L, value);
    cpu->L += 8;
}

uint64_t nd500_pop_doubleword(Nd500Cpu* cpu) {
    cpu->L -= 8;
    return nd500_read_memory_64(cpu, cpu->L);
}


/* ============================================================================
 * BITFIELD OPERATION HELPERS
 * ============================================================================
 */

uint32_t nd500_extract_bitfield(uint32_t value, uint8_t offset, uint8_t width) {
    if (width == 0 || width > 32 || offset > 31) {
        return 0;
    }

    /* Create mask with 'width' bits set */
    uint32_t mask = (width == 32) ? 0xFFFFFFFF : ((1u << width) - 1);

    /* Shift value right by offset and mask */
    return (value >> offset) & mask;
}

uint32_t nd500_insert_bitfield(uint32_t dest, uint32_t src, uint8_t offset, uint8_t width) {
    if (width == 0 || width > 32 || offset > 31) {
        return dest;
    }

    /* Create mask with 'width' bits set */
    uint32_t mask = (width == 32) ? 0xFFFFFFFF : ((1u << width) - 1);

    /* Clear destination bits, mask source, shift and OR */
    dest &= ~(mask << offset);      /* Clear dest bits */
    src &= mask;                     /* Mask source */
    dest |= (src << offset);         /* Insert */

    return dest;
}

void nd500_set_bit(Nd500Cpu* cpu, uint32_t addr, uint8_t bit_offset) {
    uint8_t byte_val = nd500_read_memory_8(cpu, addr);
    byte_val |= (1u << bit_offset);
    nd500_write_memory_8(cpu, addr, byte_val);
}

void nd500_clear_bit(Nd500Cpu* cpu, uint32_t addr, uint8_t bit_offset) {
    uint8_t byte_val = nd500_read_memory_8(cpu, addr);
    byte_val &= ~(1u << bit_offset);
    nd500_write_memory_8(cpu, addr, byte_val);
}

bool nd500_test_bit(Nd500Cpu* cpu, uint32_t addr, uint8_t bit_offset) {
    uint8_t byte_val = nd500_read_memory_8(cpu, addr);
    return (byte_val & (1u << bit_offset)) != 0;
}


/* ============================================================================
 * LOOP OPERATION HELPERS
 * ============================================================================
 */

bool nd500_loop_decrement_and_test(uint32_t* counter) {
    (*counter)--;
    return (*counter != 0);
}


/* ============================================================================
 * PACKED ARITHMETIC HELPERS
 * ============================================================================
 */

uint32_t nd500_packed_add(uint32_t a, uint32_t b, Nd500DataType element_type) {
    uint32_t result = 0;

    switch (element_type) {
        case ND500_DTYPE_BYTE: {
            /* 4 bytes per word - add each byte independently */
            for (int i = 0; i < 4; i++) {
                uint8_t a_byte = (a >> (i * 8)) & 0xFF;
                uint8_t b_byte = (b >> (i * 8)) & 0xFF;
                uint8_t sum = a_byte + b_byte;
                result |= ((uint32_t)sum << (i * 8));
            }
            break;
        }
        case ND500_DTYPE_HALFWORD: {
            /* 2 halfwords per word - add each halfword independently */
            uint16_t a_lo = a & 0xFFFF;
            uint16_t a_hi = (a >> 16) & 0xFFFF;
            uint16_t b_lo = b & 0xFFFF;
            uint16_t b_hi = (b >> 16) & 0xFFFF;

            result = ((uint32_t)(a_lo + b_lo) & 0xFFFF) |
                    (((uint32_t)(a_hi + b_hi) & 0xFFFF) << 16);
            break;
        }
        default:
            result = a + b;  /* Fallback to normal add */
            break;
    }

    return result;
}

uint32_t nd500_packed_sub(uint32_t a, uint32_t b, Nd500DataType element_type) {
    uint32_t result = 0;

    switch (element_type) {
        case ND500_DTYPE_BYTE: {
            /* 4 bytes per word - subtract each byte independently */
            for (int i = 0; i < 4; i++) {
                uint8_t a_byte = (a >> (i * 8)) & 0xFF;
                uint8_t b_byte = (b >> (i * 8)) & 0xFF;
                uint8_t diff = a_byte - b_byte;
                result |= ((uint32_t)diff << (i * 8));
            }
            break;
        }
        case ND500_DTYPE_HALFWORD: {
            /* 2 halfwords per word - subtract each halfword independently */
            uint16_t a_lo = a & 0xFFFF;
            uint16_t a_hi = (a >> 16) & 0xFFFF;
            uint16_t b_lo = b & 0xFFFF;
            uint16_t b_hi = (b >> 16) & 0xFFFF;

            result = ((uint32_t)(a_lo - b_lo) & 0xFFFF) |
                    (((uint32_t)(a_hi - b_hi) & 0xFFFF) << 16);
            break;
        }
        default:
            result = a - b;  /* Fallback to normal subtract */
            break;
    }

    return result;
}

uint32_t nd500_packed_mul(uint32_t a, uint32_t b, Nd500DataType element_type) {
    uint32_t result = 0;

    switch (element_type) {
        case ND500_DTYPE_BYTE: {
            /* 4 bytes per word - multiply each byte independently */
            for (int i = 0; i < 4; i++) {
                uint8_t a_byte = (a >> (i * 8)) & 0xFF;
                uint8_t b_byte = (b >> (i * 8)) & 0xFF;
                uint8_t prod = a_byte * b_byte;  /* Lower 8 bits of product */
                result |= ((uint32_t)prod << (i * 8));
            }
            break;
        }
        case ND500_DTYPE_HALFWORD: {
            /* 2 halfwords per word - multiply each halfword independently */
            uint16_t a_lo = a & 0xFFFF;
            uint16_t a_hi = (a >> 16) & 0xFFFF;
            uint16_t b_lo = b & 0xFFFF;
            uint16_t b_hi = (b >> 16) & 0xFFFF;

            result = ((uint32_t)(a_lo * b_lo) & 0xFFFF) |
                    (((uint32_t)(a_hi * b_hi) & 0xFFFF) << 16);
            break;
        }
        default:
            result = a * b;  /* Fallback to normal multiply */
            break;
    }

    return result;
}

/* ============================================================================
 * STATUS REGISTER AND PRIVILEGE CHECKING
 * ============================================================================
 */

/**
 * Check if CPU is in privileged mode (PIA bit set in status register)
 */
bool nd500_is_privileged(Nd500Cpu* cpu) {
    if (!cpu) return false;

    /* PIA is bit 1 in status register (ST1, bits 0-31) */
    return (cpu->ST1 & (1U << ND500_ST_BIT_PIA)) != 0;
}

/**
 * Get status register bit value
 * Reads from 64-bit status register (ST1 = bits 0-31, ST2 = bits 32-63)
 */
bool nd500_get_status_bit(Nd500Cpu* cpu, uint8_t bit_number) {
    if (!cpu || bit_number >= 64) return false;

    if (bit_number < 32) {
        /* Bits 0-31 from ST1 - reuse existing flag function */
        return nd500_test_flag(cpu, 1U << bit_number);
    } else {
        /* Bits 32-63 from ST2 */
        return (cpu->ST2 & (1U << (bit_number - 32))) != 0;
    }
}

/**
 * Set status register bit
 */
void nd500_set_status_bit(Nd500Cpu* cpu, uint8_t bit_number) {
    if (!cpu || bit_number >= 64) return;

    if (bit_number < 32) {
        /* Bits 0-31 in ST1 - reuse existing flag function */
        nd500_set_flag(cpu, 1U << bit_number);
    } else {
        /* Bits 32-63 in ST2 */
        cpu->ST2 |= (1U << (bit_number - 32));
    }
}

/**
 * Clear status register bit
 */
void nd500_clear_status_bit(Nd500Cpu* cpu, uint8_t bit_number) {
    if (!cpu || bit_number >= 64) return;

    if (bit_number < 32) {
        /* Bits 0-31 in ST1 - reuse existing flag function */
        nd500_clear_flag(cpu, 1U << bit_number);
    } else {
        /* Bits 32-63 in ST2 */
        cpu->ST2 &= ~(1U << (bit_number - 32));
    }
}

/**
 * Check privilege and trap if not privileged
 * Returns true if privileged (safe to continue), false if trapped
 */
bool nd500_require_privilege(Nd500Cpu* cpu, uint32_t pc) {
    if (!cpu) return false;

    /* Check PIA bit (bit 1 in ST1) */
    if (!nd500_is_privileged(cpu)) {
        /* Not privileged - raise IIC (Illegal Instruction Code) trap */
        trap_illegal_instruction(cpu, pc, 0);
        return false;  /* Trapped - instruction should not continue */
    }

    return true;  /* Privileged - safe to continue */
}

/* ============================================================================
 * VALIDATION HELPERS (reduce code duplication)
 * ============================================================================
 */

const char* nd500_instr_name_str(Nd500InstrName instr) {
    switch (instr) {
        case INSTR_ADD:      return "ADD";
        case INSTR_SUBTRACT: return "SUBTRACT";
        case INSTR_MULTIPLY: return "MULTIPLY";
        case INSTR_DIVIDE:   return "DIVIDE";
        case INSTR_UDIV:     return "UDIV";
        case INSTR_CLR:      return "CLR";
        case INSTR_NEG:      return "NEG";
        case INSTR_REM:      return "REM";
        case INSTR_AXI:      return "AXI";
        case INSTR_MUL4:     return "MUL4";
        default:             return "UNKNOWN";
    }
}

bool nd500_validate_operand_count(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi,
                                   uint8_t expected_count, Nd500InstrName instr) {
    if (fi->operand_count != expected_count) {
        printf("[ERROR] %s at PC=0x%08X: Expected %u operand%s, got %u\n",
               nd500_instr_name_str(instr), fi->address, expected_count,
               expected_count == 1 ? "" : "s", fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return false;
    }
    return true;
}

bool nd500_validate_target_register(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi,
                                     Nd500InstrName instr) {
    if (fi->target_register < 1 || fi->target_register > 4) {
        printf("[ERROR] %s at PC=0x%08X: Invalid target register %u\n",
               nd500_instr_name_str(instr), fi->address, fi->target_register);
        trap_illegal_operand(cpu, fi->address);
        return false;
    }
    return true;
}

bool nd500_check_float_stub(const Nd500FetchedInstruction* fi, Nd500InstrName instr) {
    if (fi->uses_float_registers) {
        printf("[STUB] %s at PC=0x%08X: Float/Double not yet implemented\n",
               nd500_instr_name_str(instr), fi->address);
        return true;  /* true = is stub, caller should return */
    }
    return false;  /* false = not stub, caller can continue */
}

/* ============================================================================
 * STRING DESCRIPTOR IMPLEMENTATION
 * ============================================================================
 */

/**
 * Helper: Read operand value at specific address
 * (Avoids duplication with nd500_read_operand_value which uses Nd500OperandDecoded)
 */
static uint64_t nd500_read_value_at_address(Nd500Cpu* cpu, uint32_t addr, Nd500DataType dtype) {
    switch (dtype) {
        case ND500_DTYPE_BYTE:
            return nd500_read_memory_8(cpu, addr);
        case ND500_DTYPE_HALFWORD:
            return nd500_read_memory_16(cpu, addr);
        case ND500_DTYPE_WORD:
            return nd500_read_memory_32(cpu, addr);
        case ND500_DTYPE_DOUBLEWORD:
            return nd500_read_memory_64(cpu, addr);
        default:
            return 0;
    }
}

/**
 * Load string descriptor from memory (based on StringDescriptor.LoadFromMemory)
 */
bool nd500_load_string_descriptor(Nd500Cpu* cpu, uint32_t desc_addr,
                                   bool is_bcd_packed, bool is_ascii,
                                   Nd500StringDescriptor* desc) {
    if (!desc) return false;

    // Initialize descriptor
    desc->is_bcd_packed = is_bcd_packed;
    desc->is_ascii = is_ascii;

    // Read 8-byte descriptor from memory (little-endian)
    desc->element_count = nd500_read_memory_32(cpu, desc_addr);
    desc->base_address = nd500_read_memory_32(cpu, desc_addr + 4);

    // For BCD/ASCII operations, parse additional fields from element count word
    if (is_bcd_packed || is_ascii) {
        uint32_t flags = desc->element_count;

        // Extract sign representation (bits 26-24)
        desc->sign_repr = (Nd500SignRepresentation)((flags >> 24) & 0x07);

        // Extract scaling factor (bits 23-18) - 6-bit signed value
        int scaling = (int)((flags >> 18) & 0x3F);
        if (scaling >= 32) scaling -= 64;  // Convert to signed (-32..+31)
        desc->scaling_factor = (int8_t)scaling;

        // Extract field width (bits 17-13) - 5-bit value
        desc->field_width = (uint8_t)((flags >> 13) & 0x1F);

        // Actual element count is in bits 12-0 (13 bits)
        desc->element_count = flags & 0x1FFF;
    }

    return true;
}

/**
 * Get element address from descriptor (based on StringDescriptor.GetElementAddress)
 */
uint32_t nd500_string_get_element_address(const Nd500StringDescriptor* desc, uint32_t index) {
    if (!desc) return 0;
    if (index >= desc->element_count) return 0;  // Out of range

    if (desc->is_bcd_packed) {
        // For BCD packed, calculate address based on field width (nibbles)
        uint32_t bytes_per_element = (desc->field_width + 1) / 2;  // Round up
        return desc->base_address + (index * bytes_per_element);
    } else if (desc->is_ascii) {
        // For ASCII, each element is typically 1 byte
        return desc->base_address + index;
    } else {
        // Standard descriptor addressing (byte-indexed)
        return desc->base_address + index;
    }
}

/**
 * Read element value from string (based on ReadElementValue)
 */
uint64_t nd500_string_read_element(Nd500Cpu* cpu, const Nd500StringDescriptor* desc,
                                    uint32_t index, Nd500DataType dtype) {
    if (!desc || !cpu) return 0;

    uint32_t addr = nd500_string_get_element_address(desc, index);
    if (addr == 0) return 0;  // Invalid index

    return nd500_read_value_at_address(cpu, addr, dtype);
}

/**
 * Validate string descriptor (based on StringDescriptor.IsValid)
 */
bool nd500_string_descriptor_is_valid(const Nd500StringDescriptor* desc) {
    if (!desc) return false;

    // Basic validation
    if (desc->base_address == 0) return false;

    // Field width validation (for BCD/ASCII)
    if (desc->is_bcd_packed || desc->is_ascii) {
        if (desc->field_width < 0 || desc->field_width > 31) return false;
        if (desc->field_width == 0) return false;  // Empty operands cause descriptor range trap

        // Scaling factor validation
        if (desc->scaling_factor < -32 || desc->scaling_factor > 31) return false;
    }

    return true;
}

/* ============================================================================
 * BCD (Binary Coded Decimal) IMPLEMENTATION
 * ============================================================================
 */

/**
 * Helper: Check if BCD sign nibble represents negative value
 * Based on IsNegativeBcdSign from C#
 */
static bool nd500_is_negative_bcd_sign(uint8_t sign_nibble) {
    return (sign_nibble == 0x0B || sign_nibble == 0x0D);  // 1011 or 1101
}

/**
 * Helper: Get BCD sign nibble for given sign and representation
 * Based on GetBcdSignNibble from C#
 */
static uint8_t nd500_get_bcd_sign_nibble(bool is_negative, Nd500SignRepresentation sign_rep) {
    if (sign_rep == ND500_SIGN_UNSIGNED) {
        return 0x0F;  // Unsigned
    }
    return is_negative ? 0x0D : 0x0C;  // Minus (1101) or Plus (1100)
}

/**
 * Read packed BCD value from memory using descriptor
 * Based on ReadPackedBcdValue from InstructionHelpers.cs
 *
 * Converts packed BCD nibbles to int64_t value:
 * - Each byte contains 2 nibbles (4-bit BCD digits)
 * - Nibbles must be 0-9 (A-F are invalid and cause IVO trap)
 * - Sign nibble handled based on sign representation
 * - Scaling factor applied as: value * 10^(-scaling_factor)
 */
int64_t nd500_read_packed_bcd_value(Nd500Cpu* cpu, const Nd500StringDescriptor* desc) {
    if (!desc || !cpu) return 0;
    if (!desc->is_bcd_packed) {
        printf("[ERROR] nd500_read_packed_bcd_value: Descriptor is not for BCD packed operations\n");
        return 0;
    }

    if (!nd500_string_descriptor_is_valid(desc)) {
        printf("[ERROR] nd500_read_packed_bcd_value: Invalid BCD descriptor\n");
        trap_invalid_operation(cpu, cpu->PC);
        return 0;
    }

    // Calculate number of bytes needed for BCD field
    int bytes_needed = (desc->field_width + 1) / 2;  // Round up to next byte
    if (bytes_needed <= 0 || bytes_needed > 16) {
        printf("[ERROR] nd500_read_packed_bcd_value: Invalid field width %d\n", desc->field_width);
        trap_invalid_operation(cpu, cpu->PC);
        return 0;
    }

    // Read BCD data from memory
    uint8_t bcd_data[16];  // Maximum 16 bytes (32 nibbles)
    for (int i = 0; i < bytes_needed; i++) {
        bcd_data[i] = nd500_read_memory_8(cpu, desc->base_address + i);
    }

    // Convert BCD to decimal (process nibbles from right to left)
    int64_t value = 0;
    int64_t multiplier = 1;
    int total_nibbles = 0;

    // Process bytes from most significant to least significant
    for (int byte_idx = bytes_needed - 1; byte_idx >= 0; byte_idx--) {
        uint8_t bcd_byte = bcd_data[byte_idx];

        // Process both nibbles in the byte (low nibble first, then high nibble)
        for (int nibble_idx = 0; nibble_idx < 2; nibble_idx++) {
            uint8_t nibble = (bcd_byte >> (nibble_idx * 4)) & 0x0F;

            // Check if this is the sign nibble (rightmost nibble in rightmost byte)
            if (byte_idx == bytes_needed - 1 && nibble_idx == 0) {
                // This is the sign nibble - handle separately
                continue;
            }

            // Validate BCD digit (0-9 only)
            if (nibble > 9) {
                printf("[ERROR] nd500_read_packed_bcd_value: Invalid BCD digit 0x%X at byte %d nibble %d\n",
                       nibble, byte_idx, nibble_idx);
                trap_invalid_operation(cpu, cpu->PC);
                return 0;
            }

            value += nibble * multiplier;
            multiplier *= 10;
            total_nibbles++;

            // Prevent overflow (max ~18 digits for int64_t)
            if (total_nibbles > 18) {
                printf("[WARN] nd500_read_packed_bcd_value: BCD value too large, truncating\n");
                break;
            }
        }
    }

    // Handle sign nibble (rightmost nibble in rightmost byte)
    uint8_t sign_nibble = bcd_data[bytes_needed - 1] & 0x0F;
    if (nd500_is_negative_bcd_sign(sign_nibble)) {
        value = -value;
    }

    // Apply scaling factor: value * 10^(-scaling_factor)
    if (desc->scaling_factor != 0) {
        double scale_factor = pow(10.0, -(double)desc->scaling_factor);
        value = (int64_t)((double)value * scale_factor);
    }

    return value;
}

/**
 * Write packed BCD value to memory using descriptor
 * Based on WritePackedBcdValue from InstructionHelpers.cs
 *
 * Converts int64_t value to packed BCD nibbles:
 * - Each byte contains 2 nibbles (4-bit BCD digits)
 * - Sign nibble added based on sign representation
 * - Scaling factor applied as: value * 10^(scaling_factor)
 */
void nd500_write_packed_bcd_value(Nd500Cpu* cpu, const Nd500StringDescriptor* desc, int64_t value) {
    if (!desc || !cpu) return;
    if (!desc->is_bcd_packed) {
        printf("[ERROR] nd500_write_packed_bcd_value: Descriptor is not for BCD packed operations\n");
        return;
    }

    if (!nd500_string_descriptor_is_valid(desc)) {
        printf("[ERROR] nd500_write_packed_bcd_value: Invalid BCD descriptor\n");
        trap_invalid_operation(cpu, cpu->PC);
        return;
    }

    // Apply scaling factor: value * 10^(scaling_factor)
    if (desc->scaling_factor != 0) {
        double scale_factor = pow(10.0, (double)desc->scaling_factor);
        value = (int64_t)((double)value * scale_factor);
    }

    // Get absolute value and sign
    bool is_negative = (value < 0);
    int64_t abs_value = is_negative ? -value : value;

    // Calculate number of bytes needed
    int bytes_needed = (desc->field_width + 1) / 2;
    if (bytes_needed <= 0 || bytes_needed > 16) {
        printf("[ERROR] nd500_write_packed_bcd_value: Invalid field width %d\n", desc->field_width);
        trap_invalid_operation(cpu, cpu->PC);
        return;
    }

    // Initialize BCD data buffer
    uint8_t bcd_data[16] = {0};  // Zero-initialize

    // Convert to BCD digits (process from least significant)
    int digit_count = 0;
    int64_t remaining = abs_value;
    while (remaining > 0 && digit_count < desc->field_width) {
        int digit = remaining % 10;
        int byte_idx = digit_count / 2;
        int nibble_idx = digit_count % 2;

        bcd_data[byte_idx] |= (digit << (nibble_idx * 4));

        remaining /= 10;
        digit_count++;
    }

    // Add sign nibble (rightmost nibble in rightmost byte)
    int sign_byte = bytes_needed - 1;
    uint8_t sign_nibble = nd500_get_bcd_sign_nibble(is_negative, desc->sign_repr);
    bcd_data[sign_byte] |= sign_nibble;  // Sign is in low nibble

    // Write BCD data to memory
    for (int i = 0; i < bytes_needed; i++) {
        nd500_write_memory_8(cpu, desc->base_address + i, bcd_data[i]);
    }
}

/* ============================================================================
 * FLOATING-POINT CONVERSION (ND-500 ↔ IEEE 754 ↔ Integer)
 * ============================================================================
 */

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
#define ND500_DOUBLE_EXPONENT_MASK  0x7F80000000000000ull  // Bits 62-54 (9 bits)
#define ND500_DOUBLE_MANTISSA_MASK  0x003FFFFFFFFFFFFFull  // Bits 53-0 (54 bits)
#define ND500_DOUBLE_EXPONENT_BIAS  256
#define ND500_DOUBLE_EXPONENT_SHIFT 54
#define ND500_DOUBLE_MANTISSA_BITS  54

// Helper: Count leading zeros (for normalization)
static inline int count_leading_zeros_32(uint32_t x) {
    if (x == 0) return 32;
    return __builtin_clz(x);
}

static inline int count_leading_zeros_64(uint64_t x) {
    if (x == 0) return 64;
    return __builtin_clzll(x);
}

/**
 * Convert int32 to ND-500 single precision float
 * Based on ND500Float.FromInt32 from C#
 */
uint32_t nd500_float_from_int32(int32_t value) {
    if (value == 0) return 0;

    // Extract sign
    bool sign = (value < 0);
    uint32_t abs_value = (uint32_t)(sign ? -value : value);

    // Find highest set bit (normalize)
    int highest_bit = 31 - count_leading_zeros_32(abs_value);

    // Calculate exponent (bias 256)
    int exponent = highest_bit + ND500_FLOAT_EXPONENT_BIAS;

    // Calculate mantissa (remove implicit leading 1)
    uint32_t mantissa = (abs_value << (31 - highest_bit)) & ND500_FLOAT_MANTISSA_MASK;

    // Combine sign, exponent, mantissa
    uint32_t result = 0;
    if (sign) result |= ND500_FLOAT_SIGN_MASK;
    result |= (uint32_t)(exponent << ND500_FLOAT_EXPONENT_SHIFT);
    result |= mantissa;

    return result;
}

/**
 * Convert ND-500 single precision float to int32 (truncated toward zero)
 *
 * Per ND-500 Reference Manual 7.2.5:
 * - Format: sign(1) | exponent(9) | mantissa(22)
 * - Value = sign * 2^(exponent - 256) * M
 * - M = 0.1mmmmm... (binary) where mantissa bits follow implicit 0.1
 * - M range: 0.5 <= M < 1.0
 * - If exponent = 0, value is exactly zero
 */
int32_t nd500_float_to_int32(uint32_t nd500_bits) {
    // Check for zero (exponent = 0 means exactly zero)
    uint32_t exponent = (nd500_bits & ND500_FLOAT_EXPONENT_MASK) >> ND500_FLOAT_EXPONENT_SHIFT;
    if (exponent == 0) return 0;

    // Extract components
    bool sign = (nd500_bits & ND500_FLOAT_SIGN_MASK) != 0;
    uint32_t mantissa = nd500_bits & ND500_FLOAT_MANTISSA_MASK;

    // Calculate actual exponent (e = exponent - 256)
    int actual_exp = (int)exponent - ND500_FLOAT_EXPONENT_BIAS;

    // Build significand with implicit bit at position 22
    // M = 0.1mmmmm... = (1 << 22 | mantissa) / 2^23
    // The implicit 1 is at bit position 22, mantissa fills bits 21-0
    uint32_t significand = (1U << ND500_FLOAT_MANTISSA_BITS) | mantissa;

    // For integer conversion:
    // value = 2^actual_exp * M = significand * 2^(actual_exp - 23)
    // We need floor(value) for truncation toward zero
    //
    // Shift amount to move binary point to position 0:
    // Significand is 23 bits (implicit 1 at bit 22 + 22 mantissa bits)
    // So shift = 23 - actual_exp to get integer part
    int shift = (ND500_FLOAT_MANTISSA_BITS + 1) - actual_exp;

    uint32_t int_value;
    if (shift >= 32) {
        // Value < 1, truncates to 0
        return 0;
    } else if (shift > 0) {
        // Normal case: shift right to get integer part
        int_value = significand >> shift;
    } else if (shift > -32) {
        // Large number: shift left
        int_value = significand << (-shift);
    } else {
        // Overflow - value too large for int32
        int_value = 0x7FFFFFFFu;
    }

    return sign ? -(int32_t)int_value : (int32_t)int_value;
}

/**
 * Convert ND-500 float to IEEE 754 single precision
 *
 * ND-500: value = 2^(exp - 256) * M, where M = 0.5 + mantissa/2^23, M in [0.5, 1.0)
 * IEEE:   value = 2^(exp - 127) * M, where M = 1 + mantissa/2^23, M in [1.0, 2.0)
 *
 * Since M_ieee = 2 * M_nd (both normalized), the exponent adjusts by -1:
 *   ieee_exp = nd_exp - 256 + 127 - 1 = nd_exp - 130
 * And mantissa shifts left (22 bits -> 23 bits):
 *   ieee_mantissa = nd_mantissa << 1
 */
float nd500_float_to_ieee754(uint32_t nd500_bits) {
    // Check for zero (exponent = 0 means exactly zero)
    uint32_t exponent = (nd500_bits & ND500_FLOAT_EXPONENT_MASK) >> ND500_FLOAT_EXPONENT_SHIFT;
    if (exponent == 0) return 0.0f;

    // Extract components
    bool sign = (nd500_bits & ND500_FLOAT_SIGN_MASK) != 0;
    uint32_t mantissa = nd500_bits & ND500_FLOAT_MANTISSA_MASK;

    // Convert exponent: ieee_exp = nd_exp - 130
    // (accounts for bias change 256->127 and mantissa range 0.5-1 -> 1-2)
    int ieee_exponent = (int)exponent - 130;

    // Check for underflow/overflow
    if (ieee_exponent <= 0) {
        // Underflow to zero
        return sign ? -0.0f : 0.0f;
    }
    if (ieee_exponent >= 255) {
        // Overflow to infinity
        union { uint32_t u; float f; } conv;
        conv.u = sign ? 0xFF800000u : 0x7F800000u;
        return conv.f;
    }

    // Build IEEE bits
    uint32_t ieee_bits = 0;
    if (sign) ieee_bits |= 0x80000000u;
    ieee_bits |= ((uint32_t)ieee_exponent << 23);

    // Convert mantissa: 22 bits -> 23 bits (shift left by 1)
    ieee_bits |= (mantissa << 1);

    // Convert bits to float
    union { uint32_t u; float f; } conv;
    conv.u = ieee_bits;
    return conv.f;
}

/**
 * Convert IEEE 754 float to ND-500 single precision
 *
 * IEEE:   value = 2^(exp - 127) * M, where M = 1 + mantissa/2^23, M in [1.0, 2.0)
 * ND-500: value = 2^(exp - 256) * M, where M = 0.5 + mantissa/2^23, M in [0.5, 1.0)
 *
 * Since M_nd = M_ieee / 2, the exponent adjusts by +1:
 *   nd_exp = ieee_exp + 256 - 127 + 1 = ieee_exp + 130
 * And mantissa shifts right (23 bits -> 22 bits):
 *   nd_mantissa = ieee_mantissa >> 1
 */
uint32_t nd500_float_from_ieee754(float ieee_value) {
    if (ieee_value == 0.0f) return 0;

    // Convert float to bits
    union { float f; uint32_t u; } conv;
    conv.f = ieee_value;
    uint32_t ieee_bits = conv.u;

    // Extract IEEE 754 components
    bool sign = (ieee_bits & 0x80000000u) != 0;
    uint32_t ieee_exponent = (ieee_bits >> 23) & 0xFF;
    uint32_t ieee_mantissa = ieee_bits & 0x7FFFFF;

    // Handle special cases
    if (ieee_exponent == 0) {
        // Denormalized number or zero - treat as zero for ND-500
        return 0;
    }
    if (ieee_exponent == 255) {
        // Infinity or NaN - return max/min ND-500 value
        uint32_t max_exp = 511;  // Max 9-bit exponent
        uint32_t nd500_bits = (max_exp << ND500_FLOAT_EXPONENT_SHIFT) | ND500_FLOAT_MANTISSA_MASK;
        if (sign) nd500_bits |= ND500_FLOAT_SIGN_MASK;
        return nd500_bits;
    }

    // Convert exponent: nd_exp = ieee_exp + 130
    int nd_exponent = (int)ieee_exponent + 130;

    // Check for ND-500 exponent overflow/underflow (9-bit range: 0-511)
    if (nd_exponent <= 0) {
        // Underflow to zero
        return 0;
    }
    if (nd_exponent > 511) {
        // Overflow - return max ND-500 value
        uint32_t nd500_bits = (511U << ND500_FLOAT_EXPONENT_SHIFT) | ND500_FLOAT_MANTISSA_MASK;
        if (sign) nd500_bits |= ND500_FLOAT_SIGN_MASK;
        return nd500_bits;
    }

    // Build ND-500 bits
    uint32_t nd500_bits = 0;
    if (sign) nd500_bits |= ND500_FLOAT_SIGN_MASK;
    nd500_bits |= ((uint32_t)nd_exponent << ND500_FLOAT_EXPONENT_SHIFT);

    // Convert mantissa: 23 bits -> 22 bits (shift right by 1)
    nd500_bits |= (ieee_mantissa >> 1);

    return nd500_bits;
}

/**
 * Check if ND-500 float is zero
 */
bool nd500_float_is_zero(uint32_t nd500_bits) {
    return (nd500_bits & ND500_FLOAT_EXPONENT_MASK) == 0;
}

/**
 * Check if ND-500 float is negative
 */
bool nd500_float_is_negative(uint32_t nd500_bits) {
    return (nd500_bits & ND500_FLOAT_SIGN_MASK) != 0;
}

/**
 * Convert int64 to ND-500 double precision float
 * Based on ND500Double.FromInt64 from C#
 */
uint64_t nd500_double_from_int64(int64_t value) {
    if (value == 0) return 0;

    // Extract sign
    bool sign = (value < 0);
    uint64_t abs_value = (uint64_t)(sign ? -value : value);

    // Find highest set bit (normalize)
    int highest_bit = 63 - count_leading_zeros_64(abs_value);

    // Calculate exponent (bias 256)
    int exponent = highest_bit + ND500_DOUBLE_EXPONENT_BIAS;

    // Calculate mantissa (remove implicit leading 1)
    uint64_t mantissa = (abs_value << (63 - highest_bit)) & ND500_DOUBLE_MANTISSA_MASK;

    // Combine sign, exponent, mantissa
    uint64_t result = 0;
    if (sign) result |= ND500_DOUBLE_SIGN_MASK;
    result |= ((uint64_t)exponent << ND500_DOUBLE_EXPONENT_SHIFT);
    result |= mantissa;

    return result;
}

/**
 * Convert ND-500 double precision float to int64 (truncated toward zero)
 *
 * Per ND-500 Reference Manual 7.2.6:
 * - Format: sign(1) | exponent(9) | mantissa(54)
 * - Value = sign * 2^(exponent - 256) * M
 * - M = 0.1mmmmm... (binary) where mantissa bits follow implicit 0.1
 * - M range: 0.5 <= M < 1.0
 * - If exponent = 0, value is exactly zero
 */
int64_t nd500_double_to_int64(uint64_t nd500_bits) {
    // Check for zero (exponent = 0 means exactly zero)
    uint32_t exponent = (uint32_t)((nd500_bits & ND500_DOUBLE_EXPONENT_MASK) >> ND500_DOUBLE_EXPONENT_SHIFT);
    if (exponent == 0) return 0;

    // Extract components
    bool sign = (nd500_bits & ND500_DOUBLE_SIGN_MASK) != 0;
    uint64_t mantissa = nd500_bits & ND500_DOUBLE_MANTISSA_MASK;

    // Calculate actual exponent (e = exponent - 256)
    int actual_exp = (int)exponent - ND500_DOUBLE_EXPONENT_BIAS;

    // Build significand with implicit bit at position 54
    // M = 0.1mmmmm... = (1 << 54 | mantissa) / 2^55
    // The implicit 1 is at bit position 54, mantissa fills bits 53-0
    uint64_t significand = (1ULL << ND500_DOUBLE_MANTISSA_BITS) | mantissa;

    // For integer conversion:
    // value = 2^actual_exp * M = significand * 2^(actual_exp - 55)
    // We need floor(value) for truncation toward zero
    //
    // Shift amount to move binary point to position 0:
    // Significand is 55 bits (implicit 1 at bit 54 + 54 mantissa bits)
    // So shift = 55 - actual_exp to get integer part
    int shift = (ND500_DOUBLE_MANTISSA_BITS + 1) - actual_exp;

    uint64_t int_value;
    if (shift >= 64) {
        // Value < 1, truncates to 0
        return 0;
    } else if (shift > 0) {
        // Normal case: shift right to get integer part
        int_value = significand >> shift;
    } else if (shift > -64) {
        // Large number: shift left
        int_value = significand << (-shift);
    } else {
        // Overflow - value too large for int64
        int_value = 0x7FFFFFFFFFFFFFFFull;
    }

    return sign ? -(int64_t)int_value : (int64_t)int_value;
}

/**
 * Convert ND-500 double to IEEE 754 double precision
 *
 * ND-500: value = 2^(exp - 256) * M, where M = 0.5 + mantissa/2^55, M in [0.5, 1.0)
 * IEEE:   value = 2^(exp - 1023) * M, where M = 1 + mantissa/2^52, M in [1.0, 2.0)
 *
 * Since M_ieee = 2 * M_nd (both normalized), the exponent adjusts by -1:
 *   ieee_exp = nd_exp - 256 + 1023 - 1 = nd_exp + 766
 * And mantissa shifts right (54 bits -> 52 bits):
 *   ieee_mantissa = nd_mantissa >> 2
 */
double nd500_double_to_ieee754(uint64_t nd500_bits) {
    // Check for zero (exponent = 0 means exactly zero)
    uint32_t exponent = (uint32_t)((nd500_bits & ND500_DOUBLE_EXPONENT_MASK) >> ND500_DOUBLE_EXPONENT_SHIFT);
    if (exponent == 0) return 0.0;

    // Extract components
    bool sign = (nd500_bits & ND500_DOUBLE_SIGN_MASK) != 0;
    uint64_t mantissa = nd500_bits & ND500_DOUBLE_MANTISSA_MASK;

    // Convert exponent: ieee_exp = nd_exp + 766
    // (accounts for bias change 256->1023 and mantissa range 0.5-1 -> 1-2)
    int ieee_exponent = (int)exponent + 766;

    // Check for underflow/overflow
    if (ieee_exponent <= 0) {
        // Underflow to zero
        return sign ? -0.0 : 0.0;
    }
    if (ieee_exponent >= 2047) {
        // Overflow to infinity
        union { uint64_t u; double d; } conv;
        conv.u = sign ? 0xFFF0000000000000ull : 0x7FF0000000000000ull;
        return conv.d;
    }

    // Build IEEE bits
    uint64_t ieee_bits = 0;
    if (sign) ieee_bits |= 0x8000000000000000ull;
    ieee_bits |= ((uint64_t)ieee_exponent << 52);

    // Convert mantissa: 54 bits -> 52 bits (shift right by 2)
    ieee_bits |= (mantissa >> 2);

    // Convert bits to double
    union { uint64_t u; double d; } conv;
    conv.u = ieee_bits;
    return conv.d;
}

/**
 * Convert IEEE 754 double to ND-500 double precision
 *
 * IEEE:   value = 2^(exp - 1023) * M, where M = 1 + mantissa/2^52, M in [1.0, 2.0)
 * ND-500: value = 2^(exp - 256) * M, where M = 0.5 + mantissa/2^55, M in [0.5, 1.0)
 *
 * Since M_nd = M_ieee / 2, the exponent adjusts by +1:
 *   nd_exp = ieee_exp - 1023 + 256 + 1 = ieee_exp - 766
 * And mantissa shifts left (52 bits -> 54 bits):
 *   nd_mantissa = ieee_mantissa << 2
 */
uint64_t nd500_double_from_ieee754(double ieee_value) {
    if (ieee_value == 0.0) return 0;

    // Convert double to bits
    union { double d; uint64_t u; } conv;
    conv.d = ieee_value;
    uint64_t ieee_bits = conv.u;

    // Extract IEEE 754 components
    bool sign = (ieee_bits & 0x8000000000000000ull) != 0;
    uint64_t ieee_exponent = (ieee_bits >> 52) & 0x7FF;
    uint64_t ieee_mantissa = ieee_bits & 0xFFFFFFFFFFFFFull;

    // Handle special cases
    if (ieee_exponent == 0) {
        // Denormalized number or zero - treat as zero for ND-500
        return 0;
    }
    if (ieee_exponent == 2047) {
        // Infinity or NaN - return max/min ND-500 value
        uint64_t max_exp = 511;  // Max 9-bit exponent
        uint64_t nd500_bits = (max_exp << ND500_DOUBLE_EXPONENT_SHIFT) | ND500_DOUBLE_MANTISSA_MASK;
        if (sign) nd500_bits |= ND500_DOUBLE_SIGN_MASK;
        return nd500_bits;
    }

    // Convert exponent: nd_exp = ieee_exp - 766
    int nd_exponent = (int)ieee_exponent - 766;

    // Check for ND-500 exponent overflow/underflow (9-bit range: 0-511)
    if (nd_exponent <= 0) {
        // Underflow to zero
        return 0;
    }
    if (nd_exponent > 511) {
        // Overflow - return max ND-500 value
        uint64_t nd500_bits = (511ULL << ND500_DOUBLE_EXPONENT_SHIFT) | ND500_DOUBLE_MANTISSA_MASK;
        if (sign) nd500_bits |= ND500_DOUBLE_SIGN_MASK;
        return nd500_bits;
    }

    // Build ND-500 bits
    uint64_t nd500_bits = 0;
    if (sign) nd500_bits |= ND500_DOUBLE_SIGN_MASK;
    nd500_bits |= ((uint64_t)nd_exponent << ND500_DOUBLE_EXPONENT_SHIFT);

    // Convert mantissa: 52 bits -> 54 bits (shift left by 2)
    nd500_bits |= (ieee_mantissa << 2);

    return nd500_bits;
}

/**
 * Check if ND-500 double is zero
 */
bool nd500_double_is_zero(uint64_t nd500_bits) {
    return (nd500_bits & ND500_DOUBLE_EXPONENT_MASK) == 0;
}

/**
 * Check if ND-500 double is negative
 */
bool nd500_double_is_negative(uint64_t nd500_bits) {
    return (nd500_bits & ND500_DOUBLE_SIGN_MASK) != 0;
}

/**
 * Convert ND-500 double to single (precision loss)
 * Based on ND500Double.ToSingle from C#
 */
uint32_t nd500_double_to_single(uint64_t nd500_double_bits) {
    // Simple conversion via int64 to avoid complex precision handling
    int64_t int_value = nd500_double_to_int64(nd500_double_bits);
    if (int_value < INT32_MIN || int_value > INT32_MAX) {
        // Overflow - return maximum/minimum float
        return (int_value < 0) ? 0xFFE00000u : 0x7FE00000u;
    }
    return nd500_float_from_int32((int32_t)int_value);
}

/**
 * Convert ND-500 single to double (precision extension)
 */
uint64_t nd500_single_to_double(uint32_t nd500_float_bits) {
    // Convert via int32 to maintain accuracy
    int32_t int_value = nd500_float_to_int32(nd500_float_bits);
    return nd500_double_from_int64((int64_t)int_value);
}
