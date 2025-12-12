/*
 * Test OTE (Own Trap Enable) instructions
 *
 * Tests all four OTE instructions:
 *   - ote1:= (0xFDBB) - LOAD: operand -> OTE1
 *   - ote1=: (0xFDC5) - STORE: OTE1 -> operand
 *   - ote2:= (0xFDBC) - LOAD: operand -> OTE2
 *   - ote2=: (0xFDC6) - STORE: OTE2 -> operand
 *
 * The OTE registers (64-bit total: OTE1=low, OTE2=high) control trap enable bits.
 *
 * NOTE: All tests use physical addresses within the allocated memory.
 * MMU is disabled for these unit tests - we're testing instruction logic only.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/nd500_instructions.h"
#include "../src/cpu/instruction_helpers.h"
#include "../src/machine/machine_protos.h"

#define MEMORY_SIZE (1 << 20)  /* 1MB */
#define CODE_ADDR   0x1000     /* Where we place test instructions */
#define DATA_ADDR   0x2000     /* Where we place test data */

/* Test counters */
static int tests_passed = 0;
static int tests_failed = 0;

/* Helper to run a test */
#define RUN_TEST(name) do { \
    printf("Running: %s\n", #name); \
    name(); \
} while(0)

/* Helper to check a condition */
#define CHECK(cond, msg) do { \
    if (cond) { \
        printf("  PASS: %s\n", msg); \
        tests_passed++; \
    } else { \
        printf("  FAIL: %s\n", msg); \
        tests_failed++; \
    } \
} while(0)

/* Helper to write instruction bytes using bus API */
static void write_code(Nd500Machine* m, uint32_t addr, const uint8_t* code, size_t len) {
    for (size_t i = 0; i < len; i++) {
        nd500_bus_write8(m, addr + i, code[i]);
    }
}

/* Helper to write a 32-bit word using bus API (big-endian) */
static void write_word(Nd500Machine* m, uint32_t addr, uint32_t value) {
    nd500_bus_write32(m, addr, value);
}

/* Helper to read a 32-bit word using bus API (big-endian) */
static uint32_t read_word(Nd500Machine* m, uint32_t addr) {
    return nd500_bus_read32(m, addr);
}

/* Helper to decode and execute an instruction */
static void execute_instruction(Nd500Machine* m, Nd500Cpu* cpu, const uint8_t* code, size_t code_len, uint32_t pc) {
    write_code(m, pc, code, code_len);
    cpu->PC = pc;

    Nd500FetchedInstruction fi;
    nd500_decode_at(m, pc, &fi);

    InstrExecFunc func = g_instr_exec_table[fi.opcode];
    if (func) {
        func(cpu, &fi);
    } else {
        printf("  ERROR: No dispatch function for opcode 0x%04X!\n", fi.opcode);
    }
}

/*
 * Test OTE1:= (LOAD) - operand -> OTE1
 * Opcode: 0xFDBB
 */
void test_ote1_load_register(void) {
    printf("\n=== Test: ote1:= with REGISTER operand ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    /* Set I1 to a known value */
    cpu.I[0] = 0xDEADBEEF;
    cpu.OTE1 = 0x00000000;  /* Clear OTE1 */
    cpu.ST1 = 0;  /* Clear flags */

    printf("  Initial: I1=0x%08X, OTE1=0x%08X\n", cpu.I[0], cpu.OTE1);

    /* Instruction: ote1:= I1 (FD BB D0) */
    uint8_t code[] = { 0xFD, 0xBB, 0xD0 };  /* D0 = REGISTER I1 */
    execute_instruction(&m, &cpu, code, sizeof(code), CODE_ADDR);

    printf("  After: OTE1=0x%08X, I1=0x%08X\n", cpu.OTE1, cpu.I[0]);

    CHECK(cpu.OTE1 == 0xDEADBEEF, "OTE1 loaded from I1");
    CHECK(cpu.I[0] == 0xDEADBEEF, "I1 unchanged");
    CHECK(!(cpu.ST1 & ND500_FLAG_Z), "Z flag clear (non-zero)");
    CHECK(cpu.ST1 & ND500_FLAG_S, "S flag set (negative)");

    nd500_machine_free(&m);
}

void test_ote1_load_memory(void) {
    printf("\n=== Test: ote1:= with ABSOLUTE memory operand ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    /* Write a value to memory at DATA_ADDR using bus API */
    uint32_t test_val = 0x12345678;
    write_word(&m, DATA_ADDR, test_val);

    cpu.OTE1 = 0x00000000;
    cpu.ST1 = 0;

    printf("  Memory[0x%X] = 0x%08X\n", DATA_ADDR, test_val);
    printf("  Initial OTE1 = 0x%08X\n", cpu.OTE1);

    /* Instruction: ote1:= $0x2000 (FD BB C4 00 00 20 00) */
    uint8_t code[] = { 0xFD, 0xBB, 0xC4, 0x00, 0x00, 0x20, 0x00 };
    execute_instruction(&m, &cpu, code, sizeof(code), CODE_ADDR);

    printf("  After: OTE1=0x%08X\n", cpu.OTE1);

    CHECK(cpu.OTE1 == test_val, "OTE1 loaded from memory");

    nd500_machine_free(&m);
}

void test_ote1_load_zero(void) {
    printf("\n=== Test: ote1:= loading zero (Z flag) ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    cpu.I[0] = 0x00000000;
    cpu.OTE1 = 0xFFFFFFFF;  /* Start with non-zero */
    cpu.ST1 = 0;

    /* Instruction: ote1:= I1 */
    uint8_t code[] = { 0xFD, 0xBB, 0xD0 };
    execute_instruction(&m, &cpu, code, sizeof(code), CODE_ADDR);

    CHECK(cpu.OTE1 == 0, "OTE1 set to zero");
    CHECK(cpu.ST1 & ND500_FLAG_Z, "Z flag set (zero)");
    CHECK(!(cpu.ST1 & ND500_FLAG_S), "S flag clear (positive)");

    nd500_machine_free(&m);
}

/*
 * Test OTE1=: (STORE) - OTE1 -> operand
 * Opcode: 0xFDC5
 */
void test_ote1_store_register(void) {
    printf("\n=== Test: ote1=: to REGISTER operand ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    cpu.OTE1 = 0xCAFEBABE;
    cpu.I[0] = 0x00000000;  /* Clear destination */
    cpu.ST1 = 0;

    printf("  Initial: OTE1=0x%08X, I1=0x%08X\n", cpu.OTE1, cpu.I[0]);

    /* Instruction: ote1=: I1 (FD C5 D0) */
    uint8_t code[] = { 0xFD, 0xC5, 0xD0 };
    execute_instruction(&m, &cpu, code, sizeof(code), CODE_ADDR);

    printf("  After: I1=0x%08X, OTE1=0x%08X\n", cpu.I[0], cpu.OTE1);

    CHECK(cpu.I[0] == 0xCAFEBABE, "I1 set from OTE1");
    CHECK(cpu.OTE1 == 0xCAFEBABE, "OTE1 unchanged");
    CHECK(!(cpu.ST1 & ND500_FLAG_Z), "Z flag clear");
    CHECK(cpu.ST1 & ND500_FLAG_S, "S flag set (negative)");

    nd500_machine_free(&m);
}

void test_ote1_store_memory(void) {
    printf("\n=== Test: ote1=: to ABSOLUTE memory operand ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    cpu.OTE1 = 0xABCD1234;

    /* Clear destination memory using bus API */
    write_word(&m, DATA_ADDR, 0x00000000);

    printf("  OTE1 = 0x%08X\n", cpu.OTE1);

    /* Instruction: ote1=: $0x2000 (FD C5 C4 00 00 20 00) */
    uint8_t code[] = { 0xFD, 0xC5, 0xC4, 0x00, 0x00, 0x20, 0x00 };
    execute_instruction(&m, &cpu, code, sizeof(code), CODE_ADDR);

    /* Read memory using bus API */
    uint32_t mem_val = read_word(&m, DATA_ADDR);

    printf("  Memory[0x%X] = 0x%08X\n", DATA_ADDR, mem_val);

    CHECK(mem_val == 0xABCD1234, "OTE1 stored to memory");

    nd500_machine_free(&m);
}

/*
 * Test OTE2:= (LOAD) - operand -> OTE2
 * Opcode: 0xFDBC
 */
void test_ote2_load_register(void) {
    printf("\n=== Test: ote2:= with REGISTER operand ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    cpu.I[3] = 0x1F1F1F1F;  /* I4 */
    cpu.OTE2 = 0x00000000;
    cpu.ST1 = 0;

    printf("  Initial: I4=0x%08X, OTE2=0x%08X\n", cpu.I[3], cpu.OTE2);

    /* Instruction: ote2:= I4 (FD BC D3) - this is the exact instruction from trace! */
    uint8_t code[] = { 0xFD, 0xBC, 0xD3 };  /* D3 = REGISTER I4 */
    execute_instruction(&m, &cpu, code, sizeof(code), CODE_ADDR);

    printf("  After: OTE2=0x%08X, I4=0x%08X\n", cpu.OTE2, cpu.I[3]);

    CHECK(cpu.OTE2 == 0x1F1F1F1F, "OTE2 loaded from I4");
    CHECK(cpu.I[3] == 0x1F1F1F1F, "I4 unchanged (critical - was bug in C#)");

    nd500_machine_free(&m);
}

void test_ote2_load_memory(void) {
    printf("\n=== Test: ote2:= with ABSOLUTE memory operand ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    /* Write a value to memory using bus API */
    uint32_t test_val = 0x87654321;
    write_word(&m, DATA_ADDR, test_val);

    cpu.OTE2 = 0x00000000;

    /* Instruction: ote2:= $0x2000 */
    uint8_t code[] = { 0xFD, 0xBC, 0xC4, 0x00, 0x00, 0x20, 0x00 };
    execute_instruction(&m, &cpu, code, sizeof(code), CODE_ADDR);

    CHECK(cpu.OTE2 == test_val, "OTE2 loaded from memory");

    nd500_machine_free(&m);
}

void test_ote2_load_zero(void) {
    printf("\n=== Test: ote2:= loading zero (Z flag) ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    cpu.I[0] = 0x00000000;
    cpu.OTE2 = 0xFFFFFFFF;
    cpu.ST1 = 0;

    /* Instruction: ote2:= I1 */
    uint8_t code[] = { 0xFD, 0xBC, 0xD0 };
    execute_instruction(&m, &cpu, code, sizeof(code), CODE_ADDR);

    CHECK(cpu.OTE2 == 0, "OTE2 set to zero");
    CHECK(cpu.ST1 & ND500_FLAG_Z, "Z flag set");
    CHECK(!(cpu.ST1 & ND500_FLAG_S), "S flag clear");

    nd500_machine_free(&m);
}

/*
 * Test OTE2=: (STORE) - OTE2 -> operand
 * Opcode: 0xFDC6
 */
void test_ote2_store_register(void) {
    printf("\n=== Test: ote2=: to REGISTER operand ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    cpu.OTE2 = 0xFEEDFACE;
    cpu.I[1] = 0x00000000;  /* I2 */
    cpu.ST1 = 0;

    printf("  Initial: OTE2=0x%08X, I2=0x%08X\n", cpu.OTE2, cpu.I[1]);

    /* Instruction: ote2=: I2 (FD C6 D1) */
    uint8_t code[] = { 0xFD, 0xC6, 0xD1 };
    execute_instruction(&m, &cpu, code, sizeof(code), CODE_ADDR);

    printf("  After: I2=0x%08X, OTE2=0x%08X\n", cpu.I[1], cpu.OTE2);

    CHECK(cpu.I[1] == 0xFEEDFACE, "I2 set from OTE2");
    CHECK(cpu.OTE2 == 0xFEEDFACE, "OTE2 unchanged");
    CHECK(!(cpu.ST1 & ND500_FLAG_Z), "Z flag clear");
    CHECK(cpu.ST1 & ND500_FLAG_S, "S flag set (negative)");

    nd500_machine_free(&m);
}

void test_ote2_store_memory(void) {
    printf("\n=== Test: ote2=: to ABSOLUTE memory operand ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    cpu.OTE2 = 0x11223344;

    /* Clear destination memory using bus API */
    write_word(&m, DATA_ADDR, 0x00000000);

    /* Instruction: ote2=: $0x2000 */
    uint8_t code[] = { 0xFD, 0xC6, 0xC4, 0x00, 0x00, 0x20, 0x00 };
    execute_instruction(&m, &cpu, code, sizeof(code), CODE_ADDR);

    /* Read memory using bus API */
    uint32_t mem_val = read_word(&m, DATA_ADDR);

    CHECK(mem_val == 0x11223344, "OTE2 stored to memory");

    nd500_machine_free(&m);
}

/*
 * Test roundtrip: LOAD then STORE
 */
void test_ote1_roundtrip(void) {
    printf("\n=== Test: OTE1 roundtrip (load then store) ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    cpu.I[0] = 0xA5A5A5A5;  /* Source: I1 */
    cpu.I[1] = 0x00000000;  /* Dest: I2 */
    cpu.OTE1 = 0x00000000;

    printf("  I1=0x%08X, I2=0x%08X, OTE1=0x%08X\n", cpu.I[0], cpu.I[1], cpu.OTE1);

    /* Step 1: ote1:= I1 - load I1 into OTE1 */
    uint8_t code1[] = { 0xFD, 0xBB, 0xD0 };
    execute_instruction(&m, &cpu, code1, sizeof(code1), CODE_ADDR);

    printf("  After ote1:= I1: OTE1=0x%08X\n", cpu.OTE1);
    CHECK(cpu.OTE1 == 0xA5A5A5A5, "OTE1 loaded");

    /* Step 2: ote1=: I2 - store OTE1 to I2 */
    uint8_t code2[] = { 0xFD, 0xC5, 0xD1 };
    execute_instruction(&m, &cpu, code2, sizeof(code2), CODE_ADDR);

    printf("  After ote1=: I2: I2=0x%08X\n", cpu.I[1]);
    CHECK(cpu.I[1] == 0xA5A5A5A5, "Roundtrip successful");

    nd500_machine_free(&m);
}

void test_ote2_roundtrip(void) {
    printf("\n=== Test: OTE2 roundtrip (load then store) ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    cpu.I[2] = 0x5A5A5A5A;  /* Source: I3 */
    cpu.I[3] = 0x00000000;  /* Dest: I4 */
    cpu.OTE2 = 0x00000000;

    printf("  I3=0x%08X, I4=0x%08X, OTE2=0x%08X\n", cpu.I[2], cpu.I[3], cpu.OTE2);

    /* Step 1: ote2:= I3 - load I3 into OTE2 */
    uint8_t code1[] = { 0xFD, 0xBC, 0xD2 };
    execute_instruction(&m, &cpu, code1, sizeof(code1), CODE_ADDR);

    printf("  After ote2:= I3: OTE2=0x%08X\n", cpu.OTE2);
    CHECK(cpu.OTE2 == 0x5A5A5A5A, "OTE2 loaded");

    /* Step 2: ote2=: I4 - store OTE2 to I4 */
    uint8_t code2[] = { 0xFD, 0xC6, 0xD3 };
    execute_instruction(&m, &cpu, code2, sizeof(code2), CODE_ADDR);

    printf("  After ote2=: I4: I4=0x%08X\n", cpu.I[3]);
    CHECK(cpu.I[3] == 0x5A5A5A5A, "Roundtrip successful");

    nd500_machine_free(&m);
}

/*
 * Test the specific bug scenario from trace analysis
 *
 * Original trace address was 0x0802D6CB (virtual), but for unit testing
 * we use physical address CODE_ADDR since MMU is disabled.
 */
void test_ote2_bug_scenario(void) {
    printf("\n=== Test: OTE2 Bug Scenario from Trace ===\n");
    printf("  This test reproduces the exact scenario that caused I4 differences\n");
    printf("  Original trace: 0x0802D6CB: ote2:= W4 (FD BC D3)\n");
    printf("  Bug: C# was STORING OTE2->I4 instead of LOADING I4->OTE2\n\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    /* Set up the exact scenario from trace */
    cpu.I[3] = 0x0000001F;  /* I4 = 0x1F (31) as in trace */
    cpu.OTE2 = 0x00000000;  /* OTE2 was 0 */
    cpu.ST1 = 0;

    printf("  Before: I4=0x%08X, OTE2=0x%08X\n", cpu.I[3], cpu.OTE2);

    /* Execute: ote2:= W4 (FD BC D3) at physical CODE_ADDR */
    uint8_t code[] = { 0xFD, 0xBC, 0xD3 };  /* ote2:= I4 */
    execute_instruction(&m, &cpu, code, sizeof(code), CODE_ADDR);

    printf("  After:  I4=0x%08X, OTE2=0x%08X\n", cpu.I[3], cpu.OTE2);

    /* C emulator (correct): OTE2 = 0x1F, I4 unchanged = 0x1F */
    /* C# emulator (buggy): OTE2 unchanged = 0, I4 = 0 (OTE2 stored to I4) */

    CHECK(cpu.OTE2 == 0x0000001F, "OTE2 should be loaded with I4 value (0x1F)");
    CHECK(cpu.I[3] == 0x0000001F, "I4 should be UNCHANGED (was 0x1F, should still be 0x1F)");

    if (cpu.I[3] == 0 && cpu.OTE2 == 0) {
        printf("\n  *** BUG DETECTED: Looks like STORE instead of LOAD! ***\n");
    }

    nd500_machine_free(&m);
}

int main(void) {
    printf("OTE (Own Trap Enable) Instruction Tests\n");
    printf("========================================\n");
    printf("NOTE: Tests use physical addresses (MMU disabled)\n");

    /* OTE1:= (LOAD) tests */
    RUN_TEST(test_ote1_load_register);
    RUN_TEST(test_ote1_load_memory);
    RUN_TEST(test_ote1_load_zero);

    /* OTE1=: (STORE) tests */
    RUN_TEST(test_ote1_store_register);
    RUN_TEST(test_ote1_store_memory);

    /* OTE2:= (LOAD) tests */
    RUN_TEST(test_ote2_load_register);
    RUN_TEST(test_ote2_load_memory);
    RUN_TEST(test_ote2_load_zero);

    /* OTE2=: (STORE) tests */
    RUN_TEST(test_ote2_store_register);
    RUN_TEST(test_ote2_store_memory);

    /* Roundtrip tests */
    RUN_TEST(test_ote1_roundtrip);
    RUN_TEST(test_ote2_roundtrip);

    /* Bug scenario test */
    RUN_TEST(test_ote2_bug_scenario);

    /* Summary */
    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
