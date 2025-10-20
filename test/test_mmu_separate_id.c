#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/machine/machine_protos.h"

/* Test result tracking */
static int tests_passed = 0;
static int tests_total = 0;

/* Helper macros */
#define TEST_START(name) \
    tests_total++; \
    printf("\nTest %d: %s\n", tests_total, name); \
    printf("─────────────────────────────────────────────\n");

#define TEST_ASSERT(condition, description) \
    if (condition) { \
        printf("✓ %s\n", description); \
    } else { \
        printf("✗ FAILED: %s\n", description); \
        return 0; \
    }

#define TEST_PASS() \
    do { \
        printf("Status: ✓ PASS\n"); \
        tests_passed++; \
        return 1; \
    } while(0)

/* Test functions */
static int test_initial_state(Nd500Cpu* cpu) {
    TEST_START("Initial MMU State - Both Disabled");
    
    int prog = nd500_mmu_is_program_enabled(cpu);
    int data = nd500_mmu_is_data_enabled(cpu);
    
    printf("Program MMU: %s\n", prog ? "ON" : "OFF");
    printf("Data MMU:    %s\n", data ? "ON" : "OFF");
    
    TEST_ASSERT(!prog, "Program MMU initially disabled");
    TEST_ASSERT(!data, "Data MMU initially disabled");
    TEST_PASS();
}

static int test_dmon_button(Nd500Cpu* cpu) {
    TEST_START("DMON Button - Enable Data MMU Only");
    
    /* Simulate clicking DMON button */
    nd500_mmu_enable_data(cpu);
    
    int prog = nd500_mmu_is_program_enabled(cpu);
    int data = nd500_mmu_is_data_enabled(cpu);
    
    printf("After DMON:\n");
    printf("  Program MMU: %s\n", prog ? "ON" : "OFF");
    printf("  Data MMU:    %s\n", data ? "ON" : "OFF");
    
    TEST_ASSERT(!prog, "Program MMU still disabled");
    TEST_ASSERT(data, "Data MMU now enabled");
    TEST_PASS();
}

static int test_pmon_button(Nd500Cpu* cpu) {
    TEST_START("PMON Button - Enable Program MMU");
    
    /* Simulate clicking PMON button (Data MMU already on from previous test) */
    nd500_mmu_enable_program(cpu);
    
    int prog = nd500_mmu_is_program_enabled(cpu);
    int data = nd500_mmu_is_data_enabled(cpu);
    
    printf("After PMON:\n");
    printf("  Program MMU: %s\n", prog ? "ON" : "OFF");
    printf("  Data MMU:    %s\n", data ? "ON" : "OFF");
    
    TEST_ASSERT(prog, "Program MMU now enabled");
    TEST_ASSERT(data, "Data MMU still enabled");
    TEST_PASS();
}

static int test_pmof_button(Nd500Cpu* cpu) {
    TEST_START("PMOF Button - Disable Program MMU Only");
    
    /* Simulate clicking PMOF button (both were on) */
    nd500_mmu_disable_program(cpu);
    
    int prog = nd500_mmu_is_program_enabled(cpu);
    int data = nd500_mmu_is_data_enabled(cpu);
    
    printf("After PMOF:\n");
    printf("  Program MMU: %s\n", prog ? "ON" : "OFF");
    printf("  Data MMU:    %s\n", data ? "ON" : "OFF");
    
    TEST_ASSERT(!prog, "Program MMU now disabled");
    TEST_ASSERT(data, "Data MMU still enabled");
    TEST_PASS();
}

static int test_dmof_button(Nd500Cpu* cpu) {
    TEST_START("DMOF Button - Disable Data MMU");
    
    /* Simulate clicking DMOF button */
    nd500_mmu_disable_data(cpu);
    
    int prog = nd500_mmu_is_program_enabled(cpu);
    int data = nd500_mmu_is_data_enabled(cpu);
    
    printf("After DMOF:\n");
    printf("  Program MMU: %s\n", prog ? "ON" : "OFF");
    printf("  Data MMU:    %s\n", data ? "ON" : "OFF");
    
    TEST_ASSERT(!prog, "Program MMU still disabled");
    TEST_ASSERT(!data, "Data MMU now disabled");
    TEST_PASS();
}

static int test_toggle_sequence(Nd500Cpu* cpu) {
    TEST_START("Button Toggle Sequence - Rapid Clicks");
    
    /* Simulate rapid button clicking */
    printf("Clicking PMON...\n");
    nd500_mmu_enable_program(cpu);
    TEST_ASSERT(nd500_mmu_is_program_enabled(cpu), "Program MMU ON after PMON");
    
    printf("Clicking PMOF...\n");
    nd500_mmu_disable_program(cpu);
    TEST_ASSERT(!nd500_mmu_is_program_enabled(cpu), "Program MMU OFF after PMOF");
    
    printf("Clicking DMON...\n");
    nd500_mmu_enable_data(cpu);
    TEST_ASSERT(nd500_mmu_is_data_enabled(cpu), "Data MMU ON after DMON");
    
    printf("Clicking DMON again (should be idempotent)...\n");
    nd500_mmu_enable_data(cpu);
    TEST_ASSERT(nd500_mmu_is_data_enabled(cpu), "Data MMU still ON");
    
    printf("Clicking DMOF...\n");
    nd500_mmu_disable_data(cpu);
    TEST_ASSERT(!nd500_mmu_is_data_enabled(cpu), "Data MMU OFF after DMOF");
    
    TEST_PASS();
}

static int test_independent_control(Nd500Cpu* cpu) {
    TEST_START("Independent I&D Control - All 4 States");
    
    /* State 1: Both OFF */
    nd500_mmu_disable_program(cpu);
    nd500_mmu_disable_data(cpu);
    printf("State 1 - Both OFF:\n");
    printf("  Program: %s, Data: %s\n", 
           nd500_mmu_is_program_enabled(cpu) ? "ON" : "OFF",
           nd500_mmu_is_data_enabled(cpu) ? "ON" : "OFF");
    TEST_ASSERT(!nd500_mmu_is_program_enabled(cpu) && !nd500_mmu_is_data_enabled(cpu),
                 "State 1: Both OFF");
    
    /* State 2: Program ON, Data OFF */
    nd500_mmu_enable_program(cpu);
    printf("State 2 - Program ON, Data OFF:\n");
    printf("  Program: %s, Data: %s\n",
           nd500_mmu_is_program_enabled(cpu) ? "ON" : "OFF",
           nd500_mmu_is_data_enabled(cpu) ? "ON" : "OFF");
    TEST_ASSERT(nd500_mmu_is_program_enabled(cpu) && !nd500_mmu_is_data_enabled(cpu),
                 "State 2: Program ON, Data OFF");
    
    /* State 3: Program OFF, Data ON */
    nd500_mmu_disable_program(cpu);
    nd500_mmu_enable_data(cpu);
    printf("State 3 - Program OFF, Data ON:\n");
    printf("  Program: %s, Data: %s\n",
           nd500_mmu_is_program_enabled(cpu) ? "ON" : "OFF",
           nd500_mmu_is_data_enabled(cpu) ? "ON" : "OFF");
    TEST_ASSERT(!nd500_mmu_is_program_enabled(cpu) && nd500_mmu_is_data_enabled(cpu),
                 "State 3: Program OFF, Data ON");
    
    /* State 4: Both ON */
    nd500_mmu_enable_program(cpu);
    printf("State 4 - Both ON:\n");
    printf("  Program: %s, Data: %s\n",
           nd500_mmu_is_program_enabled(cpu) ? "ON" : "OFF",
           nd500_mmu_is_data_enabled(cpu) ? "ON" : "OFF");
    TEST_ASSERT(nd500_mmu_is_program_enabled(cpu) && nd500_mmu_is_data_enabled(cpu),
                 "State 4: Both ON");
    
    TEST_PASS();
}

static int test_legacy_buttons(Nd500Cpu* cpu) {
    TEST_START("Legacy Enable/Disable Buttons - Both at Once");
    
    /* Start with both off */
    nd500_mmu_disable_program(cpu);
    nd500_mmu_disable_data(cpu);
    
    /* Simulate clicking legacy "Enable MMU" button */
    printf("Clicking legacy 'Enable MMU' button...\n");
    nd500_mmu_enable(cpu);
    
    int prog = nd500_mmu_is_program_enabled(cpu);
    int data = nd500_mmu_is_data_enabled(cpu);
    
    printf("After legacy enable:\n");
    printf("  Program MMU: %s\n", prog ? "ON" : "OFF");
    printf("  Data MMU:    %s\n", data ? "ON" : "OFF");
    
    TEST_ASSERT(prog && data, "Both MMUs enabled by legacy button");
    
    /* Simulate clicking legacy "Disable MMU" button */
    printf("Clicking legacy 'Disable MMU' button...\n");
    nd500_mmu_disable(cpu);
    
    prog = nd500_mmu_is_program_enabled(cpu);
    data = nd500_mmu_is_data_enabled(cpu);
    
    printf("After legacy disable:\n");
    printf("  Program MMU: %s\n", prog ? "ON" : "OFF");
    printf("  Data MMU:    %s\n", data ? "ON" : "OFF");
    
    TEST_ASSERT(!prog && !data, "Both MMUs disabled by legacy button");
    
    TEST_PASS();
}

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════\n");
    printf("  ND-500 Separate I&D MMU Button Test Suite\n");
    printf("═══════════════════════════════════════════════════════\n");
    
    /* Initialize machine and CPU */
    Nd500Machine machine;
    Nd500Cpu cpu;
    
    nd500_machine_init(&machine, 16 * 1024 * 1024);
    nd500_cpu_init(&cpu, &machine);
    
    /* Run tests in sequence (order matters - tests build on each other) */
    test_initial_state(&cpu);
    test_dmon_button(&cpu);
    test_pmon_button(&cpu);
    test_pmof_button(&cpu);
    test_dmof_button(&cpu);
    test_toggle_sequence(&cpu);
    test_independent_control(&cpu);
    test_legacy_buttons(&cpu);
    
    /* Print summary */
    printf("\n");
    printf("═══════════════════════════════════════════════════════\n");
    if (tests_passed == tests_total) {
        printf("  ✓ All %d tests PASSED!\n", tests_total);
        printf("═══════════════════════════════════════════════════════\n\n");
    } else {
        printf("  ✗ %d/%d tests PASSED (%d FAILED)\n", 
               tests_passed, tests_total, tests_total - tests_passed);
        printf("═══════════════════════════════════════════════════════\n\n");
    }
    
    /* Cleanup */
    nd500_machine_free(&machine);
    
    return (tests_passed == tests_total) ? 0 : 1;
}
