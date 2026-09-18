#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"

/*
 * Test MMU Address Translation Logic (Unit Test)
 * Tests PST/PCB accessors and MMU initialization
 * Note: Full translation testing requires full machine integration
 */

int main(void) {
    printf("ND-500 MMU Unit Test\n");
    printf("====================\n\n");

    /* Create machine and CPU with proper initialization */
    Nd500Machine machine;
    nd500_machine_init(&machine, 1024 * 1024);  /* 1MB for testing */

    Nd500Cpu cpu;
    nd500_cpu_init(&cpu, &machine);
    nd500_cpu_reset(&cpu);
    cpu.PC = 0x1000;

    /* ---------------------------------------------------------
     * TEST 1: MMU Initialization
     * --------------------------------------------------------- */
    printf("Test 1: MMU Initialization\n");
    printf("---------------------------\n");

    nd500_mmu_init(&cpu);
    int enabled = nd500_mmu_is_enabled(&cpu);
    printf("MMU enabled after init: %s (expected: no)\n", enabled ? "yes" : "no");
    printf("Status: %s\n\n", !enabled ? "PASS" : "FAIL");

    /* ---------------------------------------------------------
     * TEST 2: Enable/Disable MMU
     * --------------------------------------------------------- */
    printf("Test 2: Enable/Disable MMU\n");
    printf("---------------------------\n");

    nd500_mmu_enable(&cpu);
    int enabled_after_enable = nd500_mmu_is_enabled(&cpu);
    printf("MMU enabled: %s (expected: yes)\n", enabled_after_enable ? "yes" : "no");

    nd500_mmu_disable(&cpu);
    int enabled_after_disable = nd500_mmu_is_enabled(&cpu);
    printf("MMU disabled: %s (expected: no)\n", enabled_after_disable ? "yes" : "no");

    printf("Status: %s\n\n", (enabled_after_enable && !enabled_after_disable) ? "PASS" : "FAIL");

    /* ---------------------------------------------------------
     * TEST 3: PST Entry Set/Get
     * --------------------------------------------------------- */
    printf("Test 3: PST Entry Set/Get\n");
    printf("--------------------------\n");

    /* Set PST entry 100: Direct mode (PS_AZI), PFN=0x1234 */
    nd500_mmu_set_pst_entry(&cpu, 100, PS_AZI, 0x1234);

    PhysicalSegmentTableEntry pst = nd500_mmu_get_pst_entry(&cpu, 100);
    printf("PST[100].index_mode = %d (expected: %d=PS_AZI)\n", pst.index_mode, PS_AZI);
    printf("PST[100].pfn        = 0x%X (expected: 0x1234)\n", pst.physical_pfn);

    int pst_pass = (pst.index_mode == PS_AZI && pst.physical_pfn == 0x1234);
    printf("Status: %s\n\n", pst_pass ? "PASS" : "FAIL");

    /* ---------------------------------------------------------
     * TEST 4: PCB Capability Set/Get
     * --------------------------------------------------------- */
    printf("Test 4: PCB Capability Set/Get\n");
    printf("-------------------------------\n");

    /* Set program capability for domain 0, segment 5: PSN=100, writable */
    uint16_t pc_value = 100 | PC_DIR;
    nd500_mmu_set_program_capability(&cpu, 0, 5, pc_value);

    uint16_t pc_read = nd500_mmu_get_program_capability(&cpu, 0, 5);
    printf("PCB[0].program_capabilities[5] = 0x%04X (expected: 0x%04X)\n", pc_read, pc_value);

    /* Set data capability for domain 0, segment 7: PSN=101, writable, user accessible */
    uint16_t dc_value = 101 | DC_WRP | DC_PAC;
    nd500_mmu_set_data_capability(&cpu, 0, 7, dc_value);

    uint16_t dc_read = nd500_mmu_get_data_capability(&cpu, 0, 7);
    printf("PCB[0].data_capabilities[7]    = 0x%04X (expected: 0x%04X)\n", dc_read, dc_value);

    int pcb_pass = (pc_read == pc_value && dc_read == dc_value);
    printf("Status: %s\n\n", pcb_pass ? "PASS" : "FAIL");

    /* ---------------------------------------------------------
     * TEST 5: PCB Pointer Access
     * --------------------------------------------------------- */
    printf("Test 5: PCB Pointer Access\n");
    printf("---------------------------\n");

    ProcessControlBlock* pcb = nd500_mmu_get_pcb(&cpu, 0);
    if (pcb) {
        printf("PCB[0] pointer:     %p (valid)\n", (void*)pcb);
        printf("PCB[0].prog_cap[5]: 0x%04X (expected: 0x%04X)\n", pcb->program_capabilities[5], pc_value);
        printf("PCB[0].data_cap[7]: 0x%04X (expected: 0x%04X)\n", pcb->data_capabilities[7], dc_value);

        int ptr_pass = (pcb->program_capabilities[5] == pc_value &&
                        pcb->data_capabilities[7] == dc_value);
        printf("Status: %s\n\n", ptr_pass ? "PASS" : "FAIL");
    } else {
        printf("PCB[0] pointer:     NULL (FAIL)\n");
        printf("Status: FAIL\n\n");
    }

    /* ---------------------------------------------------------
     * TEST 6: Direct Translation (MMU Disabled)
     * --------------------------------------------------------- */
    printf("Test 6: Direct Translation (MMU Disabled)\n");
    printf("------------------------------------------\n");

    nd500_mmu_disable(&cpu);

    uint32_t vaddr = 0x12345678;
    uint32_t paddr = nd500_mmu_translate(&cpu, vaddr, 0, 0);

    printf("Virtual:  0x%08X\n", vaddr);
    printf("Physical: 0x%08X (expected: 0x%08X - direct mapping)\n", paddr, vaddr);
    printf("Status: %s\n\n", (paddr == vaddr) ? "PASS" : "FAIL");

    /* ---------------------------------------------------------
     * SUMMARY
     * --------------------------------------------------------- */
    printf("===========================================\n");
    printf("OK All MMU unit tests complete!\n");
    printf("===========================================\n");
    printf("\nNote: Full translation tests (PS_ASI/PS_ADI) require\n");
    printf("      integration with memory bus and will be tested\n");
    printf("      during Phase 5 (Memory Bus Integration).\n");

    return 0;
}
