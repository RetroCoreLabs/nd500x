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
    /* A failed decode used to be swallowed silently, which let a test "pass"
     * while the instruction under test never ran at all - the LREGBL/CTE1
     * pair did exactly that for as long as it existed. Say so loudly. */
    int rc = nd500_decode_at(m, pc, &fi);
    if (rc != 0) {
        printf("  ERROR: decode failed (rc=%d) at PC=0x%08X - instruction NOT executed\n",
               rc, pc);
        return;
    }

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
static void test_ote1_load_register(void) {
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

static void test_ote1_load_memory(void) {
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

static void test_ote1_load_zero(void) {
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
static void test_ote1_store_register(void) {
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

static void test_ote1_store_memory(void) {
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
static void test_ote2_load_register(void) {
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

static void test_ote2_load_memory(void) {
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

static void test_ote2_load_zero(void) {
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
static void test_ote2_store_register(void) {
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

static void test_ote2_store_memory(void) {
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
static void test_ote1_roundtrip(void) {
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

static void test_ote2_roundtrip(void) {
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
static void test_ote2_bug_scenario(void) {
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

/* ===================================================================
 * TEMM (Trap Enable Modification Mask) enforcement tests
 *
 * Per the ND-500 Reference Manual, a bit in OTE is modifiable only if the
 * corresponding TEMM bit is set. SETE/CLTE and OTE:= must raise an illegal
 * operand value trap (TRAP_IOV) when a non-modifiable bit is targeted, and
 * must leave OTE unchanged in that case.
 * =================================================================== */

static void test_ote1_temm_blocks_protected_bit(void) {
    printf("\n=== Test: ote1:= blocked by TEMM (protected bit) ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    cpu.OTE1 = 0x00000000;
    cpu.TEMM1 = 0xFFFFFFFF & ~0x00001000u;  /* bit 12 NOT modifiable */
    cpu.I[0] = 0x00001000;                  /* attempt to change bit 12 */
    cpu.ST1 &= ~(uint32_t)TRAP_IOV;

    uint8_t code[] = { 0xFD, 0xBB, 0xD0 };  /* ote1:= I1 */
    execute_instruction(&m, &cpu, code, sizeof(code), CODE_ADDR);

    CHECK((cpu.ST1 & (uint32_t)TRAP_IOV) != 0, "IOV trap raised for protected bit");
    CHECK(cpu.OTE1 == 0x00000000, "OTE1 unchanged after blocked write");

    nd500_machine_free(&m);
}

static void test_ote1_temm_allows_modifiable_bit(void) {
    printf("\n=== Test: ote1:= allowed when only modifiable bits change ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    cpu.OTE1 = 0x00000000;
    cpu.TEMM1 = 0xFFFFFFFF & ~0x00001000u;  /* bit 12 protected */
    cpu.I[0] = 0x00002000;                  /* only bit 13 changes (modifiable) */
    cpu.ST1 &= ~(uint32_t)TRAP_IOV;

    uint8_t code[] = { 0xFD, 0xBB, 0xD0 };  /* ote1:= I1 */
    execute_instruction(&m, &cpu, code, sizeof(code), CODE_ADDR);

    CHECK((cpu.ST1 & (uint32_t)TRAP_IOV) == 0, "no trap for modifiable bit");
    CHECK(cpu.OTE1 == 0x00002000, "OTE1 loaded with modifiable value");

    nd500_machine_free(&m);
}

static void test_sete_temm_blocks(void) {
    printf("\n=== Test: SETE blocked by TEMM ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    cpu.OTE1 = 0x00000000;
    cpu.TEMM1 = 0xFFFFFFFF & ~(1u << 5);  /* bit 5 NOT modifiable */
    cpu.I[0] = 5;                          /* SETE bit 5 */
    cpu.ST1 &= ~(uint32_t)TRAP_IOV;

    uint8_t code[] = { 0xFD, 0x39, 0xD0 };  /* SETE I1 */
    execute_instruction(&m, &cpu, code, sizeof(code), CODE_ADDR);

    CHECK((cpu.ST1 & (uint32_t)TRAP_IOV) != 0, "IOV trap raised by SETE on protected bit");
    CHECK(cpu.OTE1 == 0x00000000, "OTE1 bit not set when blocked");

    nd500_machine_free(&m);
}

static void test_sete_temm_allows(void) {
    printf("\n=== Test: SETE allowed by TEMM ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    cpu.OTE1 = 0x00000000;
    cpu.TEMM1 = 0xFFFFFFFF;  /* all modifiable */
    cpu.I[0] = 5;
    cpu.ST1 &= ~(uint32_t)TRAP_IOV;

    uint8_t code[] = { 0xFD, 0x39, 0xD0 };  /* SETE I1 */
    execute_instruction(&m, &cpu, code, sizeof(code), CODE_ADDR);

    CHECK((cpu.ST1 & (uint32_t)TRAP_IOV) == 0, "no trap for modifiable SETE bit");
    CHECK(cpu.OTE1 == (1u << 5), "OTE1 bit 5 set");

    nd500_machine_free(&m);
}

static void test_clte_temm_blocks(void) {
    printf("\n=== Test: CLTE blocked by TEMM ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    nd500_machine_init(&m, MEMORY_SIZE);
    nd500_cpu_init(&cpu, &m);
    nd500_cpu_reset(&cpu);

    cpu.OTE1 = (1u << 5);                  /* bit 5 currently set */
    cpu.TEMM1 = 0xFFFFFFFF & ~(1u << 5);  /* bit 5 NOT modifiable */
    cpu.I[0] = 5;
    cpu.ST1 &= ~(uint32_t)TRAP_IOV;

    uint8_t code[] = { 0xFD, 0x3A, 0xD0 };  /* CLTE I1 */
    execute_instruction(&m, &cpu, code, sizeof(code), CODE_ADDR);

    CHECK((cpu.ST1 & (uint32_t)TRAP_IOV) != 0, "IOV trap raised by CLTE on protected bit");
    CHECK(cpu.OTE1 == (1u << 5), "OTE1 bit unchanged when CLTE blocked");

    nd500_machine_free(&m);
}

/* ===================================================================
 * LREGBL privilege tests: CTE (mask bit 26, words 31-32 = CTE1, CTE2) must
 * NOT be loadable in non-privileged mode (manual 16.27.2).
 * =================================================================== */

/*
 * LREGBL and the CTE privilege filter.
 *
 * Both tests below used to hand-assemble the instruction as
 *     { 0xFF, 0xF6, 0x80, 0x00, 0x00, 0x00 }
 * intending "LREGBL mask=0x80000000, address=0". That is not a valid encoding
 * of a 32-bit constant operand: the decoder read it as two operands whose
 * values were BOTH zero, so LREGBL ran with mask = 0 and selected no register
 * at all. Nothing was ever loaded.
 *
 * These build the fetched instruction directly - the same approach
 * test_trap_conformance.c and test_solo_traps.c use - so the test is about
 * LREGBL's privilege filter rather than about operand encoding. Two register
 * operands: I1 carries the mask, I2 the base address.
 */
extern void nd500_instr_Lregbl(Nd500Cpu*, const Nd500FetchedInstruction*);

/* The manual's mask table is numbered in octal: CTE is bit 32 octal = 26. The
 * B30 store loop (STORERG_1 @011630) puts CTE1 at word 31 (offset 0x7C) and
 * CTE2 at word 32; nd500_regblock_words has the whole layout. (This used mask
 * bit 31 for CTE1, following the old one-bit-per-word numbering.) */
#define LREGBL_CTE1_MASK  0x04000000u        /* bit 26 selects CTE (CTE1, CTE2) */
#define LREGBL_CTE1_SLOT  (31 * 4)           /* = 0x7C with base address 0 */

static void lregbl_cte1_setup(Nd500Machine *m, Nd500Cpu *cpu,
                              Nd500FetchedInstruction *fi, int privileged) {
    nd500_machine_init(m, MEMORY_SIZE);
    nd500_cpu_init(cpu, m);
    nd500_cpu_reset(cpu);

    if (privileged) cpu->ST1 |=  ND500_FLAG_PIA;
    else            cpu->ST1 &= ~ND500_FLAG_PIA;

    cpu->CTE1 = 0xAAAAAAAA;                  /* sentinel */
    write_word(m, LREGBL_CTE1_SLOT, 0x12345678);

    /* LREGBL <mask>, <address> - both from integer registers. */
    nd500_write_integer_register(cpu, 1, LREGBL_CTE1_MASK);
    nd500_write_integer_register(cpu, 2, 0u);   /* base address */

    memset(fi, 0, sizeof *fi);
    fi->address       = CODE_ADDR;
    fi->opcode        = 0xFFF6;
    fi->operand_count = 2;
    fi->data_type     = ND500_DTYPE_WORD;
    fi->operands[0].mode = ND500_ADDR_REGISTER;  /* mask    */
    fi->operands[0].reg  = 1;
    fi->operands[1].mode = ND500_ADDR_REGISTER;  /* address */
    fi->operands[1].reg  = 2;
}

static void test_lregbl_cte1_no_leak_nonpriv(void) {
    printf("\n=== Test: LREGBL cannot write CTE1 in non-privileged mode ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    Nd500FetchedInstruction fi;
    lregbl_cte1_setup(&m, &cpu, &fi, 0);

    nd500_instr_Lregbl(&cpu, &fi);

    CHECK(cpu.CTE1 == 0xAAAAAAAA, "CTE1 unchanged (no privilege leak)");

    nd500_machine_free(&m);
}

static void test_lregbl_cte1_priv_loads(void) {
    printf("\n=== Test: LREGBL can write CTE1 in privileged mode ===\n");

    Nd500Machine m;
    Nd500Cpu cpu;
    Nd500FetchedInstruction fi;
    lregbl_cte1_setup(&m, &cpu, &fi, 1);

    nd500_instr_Lregbl(&cpu, &fi);

    CHECK(cpu.CTE1 == 0x12345678, "CTE1 loaded in privileged mode");

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

    /* TEMM enforcement tests */
    RUN_TEST(test_ote1_temm_blocks_protected_bit);
    RUN_TEST(test_ote1_temm_allows_modifiable_bit);
    RUN_TEST(test_sete_temm_blocks);
    RUN_TEST(test_sete_temm_allows);
    RUN_TEST(test_clte_temm_blocks);

    /* LREGBL privilege tests */
    RUN_TEST(test_lregbl_cte1_no_leak_nonpriv);
    RUN_TEST(test_lregbl_cte1_priv_loads);

    /* Summary */
    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");

    return tests_failed > 0 ? 1 : 0;
}
