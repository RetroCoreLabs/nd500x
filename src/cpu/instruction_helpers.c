#include "instruction_helpers.h"
#include "cpu_protos.h"
#include "../machine/machine_protos.h"
#include <stdio.h>

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
    return nd500_bus_read8(cpu->machine, vaddr);
}

void nd500_write_memory_8(Nd500Cpu* cpu, uint32_t vaddr, uint8_t value) {
    nd500_bus_write8(cpu->machine, vaddr, value);
}

uint16_t nd500_read_memory_16(Nd500Cpu* cpu, uint32_t vaddr) {
    // Read two bytes little-endian
    uint8_t b0 = nd500_bus_read8(cpu->machine, vaddr);
    uint8_t b1 = nd500_bus_read8(cpu->machine, vaddr + 1);
    return (uint16_t)b0 | ((uint16_t)b1 << 8);
}

void nd500_write_memory_16(Nd500Cpu* cpu, uint32_t vaddr, uint16_t value) {
    // Write two bytes little-endian
    nd500_bus_write8(cpu->machine, vaddr,     (uint8_t)(value & 0xFF));
    nd500_bus_write8(cpu->machine, vaddr + 1, (uint8_t)((value >> 8) & 0xFF));
}

uint32_t nd500_read_memory_32(Nd500Cpu* cpu, uint32_t vaddr) {
    // Read four bytes little-endian
    uint8_t b0 = nd500_bus_read8(cpu->machine, vaddr);
    uint8_t b1 = nd500_bus_read8(cpu->machine, vaddr + 1);
    uint8_t b2 = nd500_bus_read8(cpu->machine, vaddr + 2);
    uint8_t b3 = nd500_bus_read8(cpu->machine, vaddr + 3);
    return (uint32_t)b0 | ((uint32_t)b1 << 8) |
           ((uint32_t)b2 << 16) | ((uint32_t)b3 << 24);
}

void nd500_write_memory_32(Nd500Cpu* cpu, uint32_t vaddr, uint32_t value) {
    // Write four bytes little-endian
    nd500_bus_write8(cpu->machine, vaddr,     (uint8_t)(value & 0xFF));
    nd500_bus_write8(cpu->machine, vaddr + 1, (uint8_t)((value >> 8) & 0xFF));
    nd500_bus_write8(cpu->machine, vaddr + 2, (uint8_t)((value >> 16) & 0xFF));
    nd500_bus_write8(cpu->machine, vaddr + 3, (uint8_t)((value >> 24) & 0xFF));
}

uint64_t nd500_read_memory_64(Nd500Cpu* cpu, uint32_t vaddr) {
    // Read eight bytes little-endian
    uint32_t low  = nd500_read_memory_32(cpu, vaddr);
    uint32_t high = nd500_read_memory_32(cpu, vaddr + 4);
    return (uint64_t)low | ((uint64_t)high << 32);
}

void nd500_write_memory_64(Nd500Cpu* cpu, uint32_t vaddr, uint64_t value) {
    // Write eight bytes little-endian
    nd500_write_memory_32(cpu, vaddr,     (uint32_t)(value & 0xFFFFFFFF));
    nd500_write_memory_32(cpu, vaddr + 4, (uint32_t)((value >> 32) & 0xFFFFFFFF));
}


/* ============================================================================
 * OPERAND ACCESS HELPERS
 * ============================================================================
 */

uint8_t nd500_read_operand_byte(Nd500Cpu* cpu, const Nd500OperandDecoded* operand) {
    // Handle constants (no memory access needed)
    if (operand->mode == ND500_ADDR_CONSTANT ||
        operand->mode == ND500_ADDR_CONSTANT_SHORT) {
        return operand->data[0];  // First byte of data array
    }

    // Handle register direct
    if (operand->mode == ND500_ADDR_REGISTER) {
        uint32_t reg_value = nd500_read_integer_register(cpu, operand->reg);
        return (uint8_t)(reg_value & 0xFF);
    }

    // Memory operand - use effective address
    return nd500_bus_read8(cpu->machine, operand->effective_address);
}

uint16_t nd500_read_operand_halfword(Nd500Cpu* cpu, const Nd500OperandDecoded* operand) {
    // Handle constants (little-endian from data array)
    if (operand->mode == ND500_ADDR_CONSTANT ||
        operand->mode == ND500_ADDR_CONSTANT_SHORT) {
        return (uint16_t)operand->data[0] | ((uint16_t)operand->data[1] << 8);
    }

    // Handle register direct
    if (operand->mode == ND500_ADDR_REGISTER) {
        uint32_t reg_value = nd500_read_integer_register(cpu, operand->reg);
        return (uint16_t)(reg_value & 0xFFFF);
    }

    // Memory operand
    return nd500_read_memory_16(cpu, operand->effective_address);
}

uint32_t nd500_read_operand_word(Nd500Cpu* cpu, const Nd500OperandDecoded* operand) {
    // Handle constants (little-endian from data array)
    if (operand->mode == ND500_ADDR_CONSTANT ||
        operand->mode == ND500_ADDR_CONSTANT_SHORT) {
        return (uint32_t)operand->data[0] | ((uint32_t)operand->data[1] << 8) |
               ((uint32_t)operand->data[2] << 16) | ((uint32_t)operand->data[3] << 24);
    }

    // Handle register direct
    if (operand->mode == ND500_ADDR_REGISTER) {
        return nd500_read_integer_register(cpu, operand->reg);
    }

    // Memory operand
    return nd500_read_memory_32(cpu, operand->effective_address);
}

uint64_t nd500_read_operand_doubleword(Nd500Cpu* cpu, const Nd500OperandDecoded* operand) {
    // Handle constants (little-endian from data array - 64-bit)
    if (operand->mode == ND500_ADDR_CONSTANT ||
        operand->mode == ND500_ADDR_CONSTANT_SHORT) {
        uint64_t low = (uint32_t)operand->data[0] | ((uint32_t)operand->data[1] << 8) |
                       ((uint32_t)operand->data[2] << 16) | ((uint32_t)operand->data[3] << 24);
        uint64_t high = (uint32_t)operand->data[4] | ((uint32_t)operand->data[5] << 8) |
                        ((uint32_t)operand->data[6] << 16) | ((uint32_t)operand->data[7] << 24);
        return low | (high << 32);
    }

    // Handle register direct (double register D1-D4)
    if (operand->mode == ND500_ADDR_REGISTER) {
        return nd500_read_double_register(cpu, operand->reg);
    }

    // Memory operand
    return nd500_read_memory_64(cpu, operand->effective_address);
}

void nd500_write_operand_byte(Nd500Cpu* cpu, const Nd500OperandDecoded* operand, uint8_t value) {
    // Handle register direct
    if (operand->mode == ND500_ADDR_REGISTER) {
        // Read-modify-write: preserve upper 24 bits
        uint32_t reg_value = nd500_read_integer_register(cpu, operand->reg);
        reg_value = (reg_value & 0xFFFFFF00) | value;
        nd500_write_integer_register(cpu, operand->reg, reg_value);
        return;
    }

    // Memory operand
    nd500_bus_write8(cpu->machine, operand->effective_address, value);
}

void nd500_write_operand_halfword(Nd500Cpu* cpu, const Nd500OperandDecoded* operand, uint16_t value) {
    // Handle register direct
    if (operand->mode == ND500_ADDR_REGISTER) {
        // Read-modify-write: preserve upper 16 bits
        uint32_t reg_value = nd500_read_integer_register(cpu, operand->reg);
        reg_value = (reg_value & 0xFFFF0000) | value;
        nd500_write_integer_register(cpu, operand->reg, reg_value);
        return;
    }

    // Memory operand
    nd500_write_memory_16(cpu, operand->effective_address, value);
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

void nd500_write_operand_doubleword(Nd500Cpu* cpu, const Nd500OperandDecoded* operand, uint64_t value) {
    // Handle register direct (double register D1-D4)
    if (operand->mode == ND500_ADDR_REGISTER) {
        nd500_write_double_register(cpu, operand->reg, value);
        return;
    }

    // Memory operand
    nd500_write_memory_64(cpu, operand->effective_address, value);
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
    /* Handle constants */
    if (op->mode == ND500_ADDR_CONSTANT || op->mode == ND500_ADDR_CONSTANT_SHORT) {
        switch (dtype) {
            case ND500_DTYPE_BYTE:
                return op->data[0];
            case ND500_DTYPE_HALFWORD:
                return (uint16_t)(op->data[0] | (op->data[1] << 8));
            case ND500_DTYPE_WORD:
            case ND500_DTYPE_DOUBLEWORD:
                return (uint32_t)(op->data[0] | (op->data[1] << 8) |
                                (op->data[2] << 16) | (op->data[3] << 24));
            default:
                return 0;
        }
    }

    /* Handle registers */
    if (op->mode == ND500_ADDR_REGISTER) {
        return nd500_read_integer_register(cpu, op->reg);
    }

    /* Handle memory operands */
    switch (dtype) {
        case ND500_DTYPE_BYTE:
            return nd500_read_memory_8(cpu, op->effective_address);
        case ND500_DTYPE_HALFWORD:
            return nd500_read_memory_16(cpu, op->effective_address);
        case ND500_DTYPE_WORD:
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

    /* Handle registers */
    if (op->mode == ND500_ADDR_REGISTER) {
        nd500_write_integer_register(cpu, op->reg, (uint32_t)value);
        return;
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
