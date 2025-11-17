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
    if (!cpu || !cpu->machine) return 0;

    // Translate virtual to physical address if MMU is enabled
    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate(cpu, vaddr, 0, 0); // is_write=0, is_instruction=0

        // Check if MMU raised a trap (page fault, etc.)
        if (nd500_trap_occurred()) {
            printf("[DEBUG] Read aborted due to trap at vaddr=0x%08X\n", vaddr);
            return 0; // Abort read operation
        }
    }

    // Read four bytes little-endian from physical address
    uint8_t b0 = nd500_bus_read8(cpu->machine, paddr);
    uint8_t b1 = nd500_bus_read8(cpu->machine, paddr + 1);
    uint8_t b2 = nd500_bus_read8(cpu->machine, paddr + 2);
    uint8_t b3 = nd500_bus_read8(cpu->machine, paddr + 3);
    return (uint32_t)b0 | ((uint32_t)b1 << 8) |
           ((uint32_t)b2 << 16) | ((uint32_t)b3 << 24);
}

void nd500_write_memory_32(Nd500Cpu* cpu, uint32_t vaddr, uint32_t value) {
    if (!cpu || !cpu->machine) return;

    // Translate virtual to physical address if MMU is enabled
    uint32_t paddr = vaddr;
    if (cpu->machine->mmu_enabled) {
        paddr = nd500_mmu_translate(cpu, vaddr, 1, 0); // is_write=1, is_instruction=0

        // Check if MMU raised a trap (protect violation, page fault, etc.)
        if (nd500_trap_occurred()) {
            printf("[DEBUG] Write aborted due to trap at vaddr=0x%08X\n", vaddr);
            return; // Abort write operation
        }
    }

    // Write four bytes little-endian to physical address
    nd500_bus_write8(cpu->machine, paddr,     (uint8_t)(value & 0xFF));
    nd500_bus_write8(cpu->machine, paddr + 1, (uint8_t)((value >> 8) & 0xFF));
    nd500_bus_write8(cpu->machine, paddr + 2, (uint8_t)((value >> 16) & 0xFF));
    nd500_bus_write8(cpu->machine, paddr + 3, (uint8_t)((value >> 24) & 0xFF));
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
    // Handle CONSTANT_SHORT - value embedded in address code (low 6 bits)
    if (operand->mode == ND500_ADDR_CONSTANT_SHORT) {
        return (uint8_t)(operand->address_code & 0x3F);
    }

    // Handle CONSTANT - value in data array
    if (operand->mode == ND500_ADDR_CONSTANT) {
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
    // Handle CONSTANT_SHORT - value embedded in address code (low 6 bits)
    if (operand->mode == ND500_ADDR_CONSTANT_SHORT) {
        return (uint16_t)(operand->address_code & 0x3F);
    }

    // Handle CONSTANT - value in data array (little-endian)
    if (operand->mode == ND500_ADDR_CONSTANT) {
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
    // Handle CONSTANT_SHORT - value embedded in address code (low 6 bits)
    if (operand->mode == ND500_ADDR_CONSTANT_SHORT) {
        return (uint32_t)(operand->address_code & 0x3F);
    }

    // Handle CONSTANT - value in data array (little-endian)
    if (operand->mode == ND500_ADDR_CONSTANT) {
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

// ND-500 Float constants
#define ND500_FLOAT_SIGN_MASK      0x80000000u
#define ND500_FLOAT_EXPONENT_MASK  0x7FE00000u
#define ND500_FLOAT_MANTISSA_MASK  0x001FFFFFu
#define ND500_FLOAT_EXPONENT_BIAS  256
#define ND500_FLOAT_EXPONENT_SHIFT 22

// ND-500 Double constants  
#define ND500_DOUBLE_SIGN_MASK      0x8000000000000000ull
#define ND500_DOUBLE_EXPONENT_MASK  0x7FE0000000000000ull
#define ND500_DOUBLE_MANTISSA_MASK  0x001FFFFFFFFFFFFFull
#define ND500_DOUBLE_EXPONENT_BIAS  256
#define ND500_DOUBLE_EXPONENT_SHIFT 54

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
 * Convert ND-500 single precision float to int32 (truncated)
 * Based on ND500Float.ToInt32 from C#
 */
int32_t nd500_float_to_int32(uint32_t nd500_bits) {
    // Check for zero (exponent = 0)
    uint32_t exponent = (nd500_bits & ND500_FLOAT_EXPONENT_MASK) >> ND500_FLOAT_EXPONENT_SHIFT;
    if (exponent == 0) return 0;

    // Extract components
    bool sign = (nd500_bits & ND500_FLOAT_SIGN_MASK) != 0;
    uint32_t mantissa = nd500_bits & ND500_FLOAT_MANTISSA_MASK;

    // Calculate actual exponent
    int actual_exponent = (int)exponent - ND500_FLOAT_EXPONENT_BIAS;

    // Reconstruct integer value (add implicit leading 1)
    uint32_t value = 0x80000000u | (mantissa >> (ND500_FLOAT_EXPONENT_SHIFT - 1));

    // Shift based on exponent
    if (actual_exponent < 0) {
        value >>= -actual_exponent;
    } else if (actual_exponent > 0) {
        if (actual_exponent >= 32)
            value = 0xFFFFFFFFu; // Overflow
        else
            value <<= actual_exponent;
    }

    // Apply sign
    return sign ? -(int32_t)value : (int32_t)value;
}

/**
 * Convert ND-500 float to IEEE 754 single precision
 * Based on ND500Float.ToIeee754Single from C#
 */
float nd500_float_to_ieee754(uint32_t nd500_bits) {
    // Check for zero
    uint32_t exponent = (nd500_bits & ND500_FLOAT_EXPONENT_MASK) >> ND500_FLOAT_EXPONENT_SHIFT;
    if (exponent == 0) return 0.0f;

    // Extract components
    bool sign = (nd500_bits & ND500_FLOAT_SIGN_MASK) != 0;
    uint32_t mantissa = nd500_bits & ND500_FLOAT_MANTISSA_MASK;

    // Convert to IEEE 754 format
    uint32_t ieee_bits = 0;
    if (sign) ieee_bits |= 0x80000000u;

    // Adjust exponent from ND-500 bias (256) to IEEE 754 bias (127)
    int actual_exponent = (int)exponent - ND500_FLOAT_EXPONENT_BIAS;
    uint32_t ieee_exponent = (uint32_t)(actual_exponent + 127);
    ieee_bits |= (ieee_exponent << 23);

    // Adjust mantissa from 22+1 bits to 23 bits (IEEE 754)
    ieee_bits |= (mantissa >> 1);

    // Convert bits to float
    union { uint32_t u; float f; } conv;
    conv.u = ieee_bits;
    return conv.f;
}

/**
 * Convert IEEE 754 float to ND-500 single precision
 * Based on ND500Float.FromIeee754Single from C#
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

    // Convert to ND-500 format
    uint32_t nd500_bits = 0;
    if (sign) nd500_bits |= ND500_FLOAT_SIGN_MASK;

    // Adjust exponent from IEEE 754 bias (127) to ND-500 bias (256)
    int actual_exponent = (int)ieee_exponent - 127;
    uint32_t nd500_exponent = (uint32_t)(actual_exponent + ND500_FLOAT_EXPONENT_BIAS);
    nd500_bits |= (nd500_exponent << ND500_FLOAT_EXPONENT_SHIFT);

    // Adjust mantissa from 23 bits to 22+1 bits (ND-500)
    nd500_bits |= (ieee_mantissa << 1);

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
    result |= (uint64_t)(exponent << ND500_DOUBLE_EXPONENT_SHIFT);
    result |= mantissa;

    return result;
}

/**
 * Convert ND-500 double precision float to int64 (truncated)
 * Based on ND500Double.ToInt64 from C#
 */
int64_t nd500_double_to_int64(uint64_t nd500_bits) {
    // Check for zero (exponent = 0)
    uint32_t exponent = (uint32_t)((nd500_bits & ND500_DOUBLE_EXPONENT_MASK) >> ND500_DOUBLE_EXPONENT_SHIFT);
    if (exponent == 0) return 0;

    // Extract components
    bool sign = (nd500_bits & ND500_DOUBLE_SIGN_MASK) != 0;
    uint64_t mantissa = nd500_bits & ND500_DOUBLE_MANTISSA_MASK;

    // Calculate actual exponent
    int actual_exponent = (int)exponent - ND500_DOUBLE_EXPONENT_BIAS;

    // Reconstruct integer value (add implicit leading 1)
    uint64_t value = 0x8000000000000000ull | (mantissa >> (ND500_DOUBLE_EXPONENT_SHIFT - 1));

    // Shift based on exponent
    if (actual_exponent < 0) {
        value >>= -actual_exponent;
    } else if (actual_exponent > 0) {
        if (actual_exponent >= 64)
            value = 0xFFFFFFFFFFFFFFFFull; // Overflow
        else
            value <<= actual_exponent;
    }

    // Apply sign
    return sign ? -(int64_t)value : (int64_t)value;
}

/**
 * Convert ND-500 double to IEEE 754 double precision
 * Based on ND500Double.ToIeee754Double from C#
 */
double nd500_double_to_ieee754(uint64_t nd500_bits) {
    // Check for zero
    uint32_t exponent = (uint32_t)((nd500_bits & ND500_DOUBLE_EXPONENT_MASK) >> ND500_DOUBLE_EXPONENT_SHIFT);
    if (exponent == 0) return 0.0;

    // Extract components
    bool sign = (nd500_bits & ND500_DOUBLE_SIGN_MASK) != 0;
    uint64_t mantissa = nd500_bits & ND500_DOUBLE_MANTISSA_MASK;

    // Convert to IEEE 754 format
    uint64_t ieee_bits = 0;
    if (sign) ieee_bits |= 0x8000000000000000ull;

    // Adjust exponent from ND-500 bias (256) to IEEE 754 bias (1023)
    int actual_exponent = (int)exponent - ND500_DOUBLE_EXPONENT_BIAS;
    uint64_t ieee_exponent = (uint64_t)(actual_exponent + 1023);
    ieee_bits |= (ieee_exponent << 52);

    // Adjust mantissa from 54+1 bits to 52 bits (IEEE 754)
    ieee_bits |= (mantissa >> 2);

    // Convert bits to double
    union { uint64_t u; double d; } conv;
    conv.u = ieee_bits;
    return conv.d;
}

/**
 * Convert IEEE 754 double to ND-500 double precision
 * Based on ND500Double.FromIeee754Double from C#
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

    // Convert to ND-500 format
    uint64_t nd500_bits = 0;
    if (sign) nd500_bits |= ND500_DOUBLE_SIGN_MASK;

    // Adjust exponent from IEEE 754 bias (1023) to ND-500 bias (256)
    int actual_exponent = (int)ieee_exponent - 1023;
    uint64_t nd500_exponent = (uint64_t)(actual_exponent + ND500_DOUBLE_EXPONENT_BIAS);
    nd500_bits |= (nd500_exponent << ND500_DOUBLE_EXPONENT_SHIFT);

    // Adjust mantissa from 52 bits to 54+1 bits (ND-500)
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
