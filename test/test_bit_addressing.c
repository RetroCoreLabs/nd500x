/**
 * ND500 BIT (BI) Addressing Test
 *
 * Tests BIT-prefixed instructions and bit addressing mode:
 * - BI TEST: Test single bit against zero
 * - BI SET1: Set single bit to 1
 * - BI CLR1: Clear single bit to 0
 *
 * BIT addressing mode (per ND-500 Reference Manual 7.2.1):
 * - Index register provides BIT offset (not byte offset)
 * - Effective byte address = base + (bit_index / 8)
 * - Bit position within byte = bit_index % 8
 * - Bit 0 = LSB (rightmost bit)
 *
 * This test verifies the fix for the BI addressing bug where
 * the index was incorrectly treated as a byte offset.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/nd500_instructions.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/instruction_helpers.h"

/* nd500_dbg_set_trap_invalid is declared in machine_protos.h */

/* Test memory size */
#define MEMORY_SIZE (1 << 20)  /* 1MB */
#define CODE_ADDR   0x2000     /* Where we place test instructions */
#define DATA_ADDR   0x1000     /* Where we place test data */

/* Test result tracking */
static int tests_passed = 0;
static int tests_failed = 0;

/* Helper to print test result */
static void test_result(const char* name, bool passed, const char* details) {
    if (passed) {
        tests_passed++;
        printf("  [PASS] %s\n", name);
    } else {
        tests_failed++;
        printf("  [FAIL] %s: %s\n", name, details);
    }
}

/* Helper to write instruction bytes */
static void write_code(Nd500Machine* m, uint32_t addr, const uint8_t* code, size_t len) {
    for (size_t i = 0; i < len; i++) {
        nd500_bus_write8(m, addr + i, code[i]);
    }
}

/* Helper to execute an instruction using CPU step (proper decode/execute cycle) */
static void execute_instruction(Nd500Machine* m, Nd500Cpu* cpu, const uint8_t* code, size_t code_len, uint32_t pc) {
    (void)m;  /* cpu->machine already points to machine */
    write_code(cpu->machine, pc, code, code_len);
    cpu->PC = pc;
    nd500_cpu_step(cpu);
}

/**
 * Test BIT address calculation
 *
 * Verifies that bit index is correctly split into:
 * - Byte offset = bit_index / 8
 * - Bit position = bit_index % 8
 */
static void test_bit_address_calculation(void) {
    printf("\n=== BIT Address Calculation Tests ===\n");

    struct {
        int32_t bit_index;
        uint32_t expected_byte_offset;
        uint8_t expected_bit_position;
    } test_cases[] = {
        /* Bit 0-7: same byte, different bit positions */
        {0, 0, 0},
        {1, 0, 1},
        {2, 0, 2},
        {3, 0, 3},
        {4, 0, 4},
        {5, 0, 5},
        {6, 0, 6},
        {7, 0, 7},
        /* Bit 8-15: next byte */
        {8, 1, 0},
        {9, 1, 1},   /* The bug case: R1=9 should give byte+1, bit 1 */
        {10, 1, 2},
        {15, 1, 7},
        /* Larger offsets */
        {16, 2, 0},
        {17, 2, 1},
        {31, 3, 7},
        {32, 4, 0},
        {100, 12, 4},
        {255, 31, 7},
    };
    int num_tests = sizeof(test_cases) / sizeof(test_cases[0]);

    for (int i = 0; i < num_tests; i++) {
        int32_t bit_index = test_cases[i].bit_index;
        uint32_t expected_byte = test_cases[i].expected_byte_offset;
        uint8_t expected_bit = test_cases[i].expected_bit_position;

        /* Calculate using the same algorithm as cpu_instr.c */
        uint32_t actual_byte = (uint32_t)(bit_index >> 3);  /* bit_index / 8 */
        uint8_t actual_bit = (uint8_t)(bit_index & 0x07);   /* bit_index % 8 */

        char name[64];
        snprintf(name, sizeof(name), "bit_addr(%d)", bit_index);

        char details[128];
        snprintf(details, sizeof(details),
                 "byte: expected %u got %u, bit: expected %u got %u",
                 expected_byte, actual_byte, expected_bit, actual_bit);

        bool passed = (actual_byte == expected_byte) && (actual_bit == expected_bit);
        test_result(name, passed, details);
    }
}

/**
 * Test BIT memory read helper
 *
 * Directly tests nd500_read_operand_value with BIT type to verify
 * correct bit extraction from memory.
 */
static void test_bit_memory_read(void) {
    printf("\n=== BIT Memory Read Tests ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    /* Test data at address DATA_ADDR:
     * 0x1000: 0x00  = 00000000b (all bits 0)
     * 0x1001: 0x5F  = 01011111b (bits 0,1,2,3,4,6 set)
     * 0x1002: 0x80  = 10000000b (bit 7 set)
     * 0x1003: 0xFF  = 11111111b (all bits set)
     */
    nd500_bus_write8(&m, DATA_ADDR + 0, 0x00);
    nd500_bus_write8(&m, DATA_ADDR + 1, 0x5F);
    nd500_bus_write8(&m, DATA_ADDR + 2, 0x80);
    nd500_bus_write8(&m, DATA_ADDR + 3, 0xFF);

    struct {
        int32_t bit_index;
        uint8_t expected_bit;
        const char* desc;
    } test_cases[] = {
        /* Test byte 0x1000 (0x00) - all zeros */
        {0, 0, "bit 0 of 0x00"},
        {7, 0, "bit 7 of 0x00"},

        /* Test byte 0x1001 (0x5F = 01011111b) */
        {8, 1, "bit 0 of 0x5F"},
        {9, 1, "bit 1 of 0x5F (THE BUG CASE)"},
        {10, 1, "bit 2 of 0x5F"},
        {11, 1, "bit 3 of 0x5F"},
        {12, 1, "bit 4 of 0x5F"},
        {13, 0, "bit 5 of 0x5F"},
        {14, 1, "bit 6 of 0x5F"},
        {15, 0, "bit 7 of 0x5F"},

        /* Test byte 0x1002 (0x80 = 10000000b) */
        {16, 0, "bit 0 of 0x80"},
        {23, 1, "bit 7 of 0x80"},

        /* Test byte 0x1003 (0xFF) - all ones */
        {24, 1, "bit 0 of 0xFF"},
        {31, 1, "bit 7 of 0xFF"},
    };
    int num_tests = sizeof(test_cases) / sizeof(test_cases[0]);

    for (int i = 0; i < num_tests; i++) {
        int32_t bit_index = test_cases[i].bit_index;

        /* Create operand with computed effective address and bit position */
        Nd500OperandDecoded op = {0};
        op.mode = ND500_ADDR_ABSOLUTE;  /* Memory operand */
        op.effective_address = DATA_ADDR + (bit_index >> 3);
        op.bit_position = (uint8_t)(bit_index & 0x07);

        /* Read using BIT type */
        uint64_t value = nd500_read_operand_value(&cpu, &op, ND500_DTYPE_BIT);

        char name[64];
        snprintf(name, sizeof(name), "bit_read(idx=%d) %s",
                 bit_index, test_cases[i].desc);

        char details[128];
        snprintf(details, sizeof(details),
                 "expected %d, got %llu (addr=0x%X, bit_pos=%d)",
                 test_cases[i].expected_bit, (unsigned long long)value,
                 op.effective_address, op.bit_position);

        test_result(name, value == test_cases[i].expected_bit, details);
    }

    nd500_machine_free(&m);
}

/**
 * Test BIT memory write helper
 *
 * Directly tests nd500_write_operand_value with BIT type to verify
 * correct bit setting/clearing in memory.
 */
static void test_bit_memory_write(void) {
    printf("\n=== BIT Memory Write Tests ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    struct {
        int32_t bit_index;
        uint8_t initial_byte;
        uint64_t write_value;
        uint8_t expected_byte;
    } test_cases[] = {
        /* Set bits */
        {0, 0x00, 1, 0x01},
        {1, 0x00, 1, 0x02},
        {7, 0x00, 1, 0x80},
        {0, 0xFE, 1, 0xFF},

        /* Clear bits */
        {0, 0xFF, 0, 0xFE},
        {7, 0xFF, 0, 0x7F},

        /* Set in next byte (bit_index >= 8) */
        {8, 0x00, 1, 0x01},
        {9, 0x00, 1, 0x02},
    };
    int num_tests = sizeof(test_cases) / sizeof(test_cases[0]);

    for (int i = 0; i < num_tests; i++) {
        int32_t bit_index = test_cases[i].bit_index;
        uint32_t byte_addr = DATA_ADDR + (bit_index >> 3);

        /* Initialize memory */
        nd500_bus_write8(&m, byte_addr, test_cases[i].initial_byte);

        /* Create operand */
        Nd500OperandDecoded op = {0};
        op.mode = ND500_ADDR_ABSOLUTE;
        op.effective_address = byte_addr;
        op.bit_position = (uint8_t)(bit_index & 0x07);

        /* Write using BIT type */
        nd500_write_operand_value(&cpu, &op, test_cases[i].write_value, ND500_DTYPE_BIT);

        /* Read back */
        uint8_t result = nd500_bus_read8(&m, byte_addr);

        char name[64];
        snprintf(name, sizeof(name), "bit_write(idx=%d, val=%llu)",
                 bit_index, (unsigned long long)test_cases[i].write_value);

        char details[128];
        snprintf(details, sizeof(details),
                 "byte at 0x%X: expected 0x%02X, got 0x%02X",
                 byte_addr, test_cases[i].expected_byte, result);

        test_result(name, result == test_cases[i].expected_byte, details);
    }

    nd500_machine_free(&m);
}

/**
 * Test the DOM bug scenario using helper functions directly
 *
 * This tests the exact scenario without full instruction execution.
 */
static void test_dom_bug_scenario(void) {
    printf("\n=== DOM Bug Scenario Test ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    /* Set up exact memory contents from DOM file */
    nd500_bus_write8(&m, DATA_ADDR + 0, 0x00);
    nd500_bus_write8(&m, DATA_ADDR + 1, 0x5F);

    /* Simulate: bi test $DATA_ADDR(r1) with R1=9
     * Expected address calculation:
     *   byte_addr = DATA_ADDR + (9 / 8) = DATA_ADDR + 1
     *   bit_pos = 9 % 8 = 1
     * Byte at DATA_ADDR+1 = 0x5F = 01011111b
     * Bit 1 = 1 (not zero)
     */
    int32_t bit_index = 9;

    Nd500OperandDecoded op = {0};
    op.mode = ND500_ADDR_ABSOLUTE;
    op.effective_address = DATA_ADDR + (bit_index >> 3);
    op.bit_position = (uint8_t)(bit_index & 0x07);

    uint64_t value = nd500_read_operand_value(&cpu, &op, ND500_DTYPE_BIT);

    /* Set flags as TEST instruction would */
    nd500_set_flags_zs(&cpu, value, ND500_DTYPE_BIT);

    bool z_set = (cpu.ST1 & ND500_FLAG_Z) != 0;

    printf("  Test: bi test with bit_index=9\n");
    printf("    Memory: 0x%X=0x00, 0x%X=0x5F\n", DATA_ADDR, DATA_ADDR + 1);
    printf("    Calculated: byte_addr=0x%X, bit_pos=%d\n", op.effective_address, op.bit_position);
    printf("    Bit value read: %llu\n", (unsigned long long)value);
    printf("    Z flag: %s (should be CLEAR)\n", z_set ? "SET" : "CLEAR");

    test_result("dom_bug_scenario", !z_set && value == 1,
                "Z should be CLEAR because bit 1 of 0x5F = 1");

    nd500_machine_free(&m);
}

/**
 * Test actual BI TEST instruction execution with post-indexed addressing
 *
 * This test executes REAL instruction bytes through the full decode/execute
 * pipeline to test compute_effective_address with BIT data type.
 *
 * The existing tests manually compute bit_position, but this test exercises
 * the actual code path where compute_effective_address calculates:
 *   bit_position = 7 - (index % 8)  [counted from left/MSB]
 *
 * Instruction encoding:
 *   BI TEST = opcode 0x0041 (2 bytes)
 *   $addr(r1) = address code 0xE0 + 4-byte address (big-endian)
 */
static void test_bi_test_instruction_execution(void) {
    printf("\n=== BI TEST Instruction Execution Tests ===\n");

    /* Disable the trap on 0x00 instruction check - BI TEST opcode 0x0041 starts with 0x00 */
    nd500_dbg_set_trap_invalid(0);

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    /* Test data at DATA_ADDR (0x1000):
     * 0x1000: 0x00 = 00000000b (all bits 0)
     * 0x1001: 0x5F = 01011111b
     *         bit 7 (MSB) = 0
     *         bit 6 = 1
     *         bit 5 = 0
     *         bit 4 = 1
     *         bit 3 = 1
     *         bit 2 = 1
     *         bit 1 = 1
     *         bit 0 (LSB) = 1
     */
    nd500_bus_write8(&m, DATA_ADDR + 0, 0x00);
    nd500_bus_write8(&m, DATA_ADDR + 1, 0x5F);
    nd500_bus_write8(&m, DATA_ADDR + 2, 0x80);
    nd500_bus_write8(&m, DATA_ADDR + 3, 0xFF);

    /* BI TEST instruction: opcode 0x0041
     * In memory, 0x00xx opcodes are stored as single byte (just 0x41)
     * Operand: $DATA_ADDR(r1) = address code 0xE0 + 4-byte address
     * Full encoding: 41 E0 00 00 10 00 (6 bytes)
     */
    uint8_t bi_test_instr[] = {
        0x41,                 /* BI TEST opcode (low byte only, 0x00 prefix implicit) */
        0xE0,                 /* $addr(r1) addressing mode */
        (DATA_ADDR >> 24) & 0xFF,
        (DATA_ADDR >> 16) & 0xFF,
        (DATA_ADDR >> 8) & 0xFF,
        (DATA_ADDR) & 0xFF
    };

    struct {
        int32_t index;        /* I1 value (bit index) */
        uint8_t expected_bit; /* Expected bit value (0 or 1) */
        bool expected_z;      /* Expected Z flag (Z=1 if bit is 0) */
        const char* desc;
    } test_cases[] = {
        /* Index 0-7: byte at DATA_ADDR+0 (0x00 = all zeros) */
        { 0, 0, true,  "bit 0 of 0x00" },
        { 7, 0, true,  "bit 7 of 0x00" },

        /* Index 8-15: byte at DATA_ADDR+1 (0x5F = 01011111b)
         * Using ND-500 bit numbering (from left/MSB):
         *   index 8  -> byte_offset=1, bit_pos=7-(8%8)=7 -> bit 7 = 0
         *   index 9  -> byte_offset=1, bit_pos=7-(9%8)=6 -> bit 6 = 1
         *   index 10 -> byte_offset=1, bit_pos=7-(10%8)=5 -> bit 5 = 0
         *   index 11 -> byte_offset=1, bit_pos=7-(11%8)=4 -> bit 4 = 1
         *   index 12 -> byte_offset=1, bit_pos=7-(12%8)=3 -> bit 3 = 1
         *   index 13 -> byte_offset=1, bit_pos=7-(13%8)=2 -> bit 2 = 1 (CR!)
         *   index 14 -> byte_offset=1, bit_pos=7-(14%8)=1 -> bit 1 = 1
         *   index 15 -> byte_offset=1, bit_pos=7-(15%8)=0 -> bit 0 = 1
         */
        { 8,  0, true,  "idx=8: bit_pos=7 of 0x5F = 0" },
        { 9,  1, false, "idx=9: bit_pos=6 of 0x5F = 1 (TAB BUG CASE)" },
        { 10, 0, true,  "idx=10: bit_pos=5 of 0x5F = 0" },
        { 11, 1, false, "idx=11: bit_pos=4 of 0x5F = 1" },
        { 12, 1, false, "idx=12: bit_pos=3 of 0x5F = 1" },
        { 13, 1, false, "idx=13: bit_pos=2 of 0x5F = 1 (CR BUG CASE)" },
        { 14, 1, false, "idx=14: bit_pos=1 of 0x5F = 1" },
        { 15, 1, false, "idx=15: bit_pos=0 of 0x5F = 1" },

        /* Index 16-23: byte at DATA_ADDR+2 (0x80 = 10000000b) */
        { 16, 1, false, "idx=16: bit_pos=7 of 0x80 = 1" },
        { 17, 0, true,  "idx=17: bit_pos=6 of 0x80 = 0" },
        { 23, 0, true,  "idx=23: bit_pos=0 of 0x80 = 0" },

        /* Index 24-31: byte at DATA_ADDR+3 (0xFF = all ones) */
        { 24, 1, false, "idx=24: bit_pos=7 of 0xFF = 1" },
        { 31, 1, false, "idx=31: bit_pos=0 of 0xFF = 1" },
    };
    int num_tests = sizeof(test_cases) / sizeof(test_cases[0]);

    for (int i = 0; i < num_tests; i++) {
        /* Reset CPU state */
        nd500_cpu_reset(&cpu);
        cpu.ST1 = 0;  /* Clear all flags */
        cpu.I[0] = (uint32_t)test_cases[i].index;  /* I1 = bit index */

        /* Execute BI TEST instruction */
        execute_instruction(&m, &cpu, bi_test_instr, sizeof(bi_test_instr), CODE_ADDR);

        bool z_set = (cpu.ST1 & ND500_FLAG_Z) != 0;

        char name[80];
        snprintf(name, sizeof(name), "bi_test(I1=%d) %s",
                 test_cases[i].index, test_cases[i].desc);

        char details[128];
        snprintf(details, sizeof(details),
                 "Z flag: expected %s got %s",
                 test_cases[i].expected_z ? "SET" : "CLEAR",
                 z_set ? "SET" : "CLEAR");

        bool passed = (z_set == test_cases[i].expected_z);
        test_result(name, passed, details);

        if (!passed) {
            /* Extra debug info on failure */
            printf("    DEBUG: I1=%d, byte_offset=%d, bit_pos_formula=%d\n",
                   test_cases[i].index,
                   test_cases[i].index >> 3,
                   7 - (test_cases[i].index & 7));
            uint32_t byte_addr = DATA_ADDR + (test_cases[i].index >> 3);
            uint8_t byte_val = nd500_bus_read8(&m, byte_addr);
            printf("    DEBUG: byte at 0x%X = 0x%02X\n", byte_addr, byte_val);
        }
    }

    nd500_machine_free(&m);
}

/**
 * Test flag handling for BIT type
 *
 * Verifies nd500_set_flags_zs handles ND500_DTYPE_BIT correctly.
 */
static void test_bit_flag_handling(void) {
    printf("\n=== BIT Flag Handling Tests ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);

    struct {
        uint64_t value;
        bool expected_z;
        bool expected_s;
    } test_cases[] = {
        {0, true, false},   /* Zero bit -> Z set, S clear */
        {1, false, false},  /* One bit -> Z clear, S clear */
        /* Higher bits should be masked to 1-bit */
        {2, true, false},   /* 0b10 & 0x1 = 0 -> Z set */
        {0xFF, false, false}, /* 0xFF & 0x1 = 1 -> Z clear */
    };

    int num_tests = sizeof(test_cases) / sizeof(test_cases[0]);

    for (int i = 0; i < num_tests; i++) {
        cpu.ST1 = 0;  /* Clear all flags */

        nd500_set_flags_zs(&cpu, test_cases[i].value, ND500_DTYPE_BIT);

        bool z_set = (cpu.ST1 & ND500_FLAG_Z) != 0;
        bool s_set = (cpu.ST1 & ND500_FLAG_S) != 0;

        char name[64];
        snprintf(name, sizeof(name), "bit_flags(0x%llX)",
                 (unsigned long long)test_cases[i].value);

        char details[128];
        snprintf(details, sizeof(details),
                 "Z: expected %d got %d, S: expected %d got %d",
                 test_cases[i].expected_z, z_set,
                 test_cases[i].expected_s, s_set);

        bool passed = (z_set == test_cases[i].expected_z) &&
                      (s_set == test_cases[i].expected_s);
        test_result(name, passed, details);
    }

    nd500_machine_free(&m);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("ND-500 BIT Addressing Tests\n");
    printf("===========================\n");

    /* Run tests */
    test_bit_address_calculation();
    test_bit_memory_read();
    test_bit_memory_write();
    test_dom_bug_scenario();
    test_bi_test_instruction_execution();  /* NEW: Tests actual instruction execution */
    test_bit_flag_handling();

    /* Summary */
    printf("\n===========================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
