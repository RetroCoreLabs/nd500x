#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "../src/cpu/cpu_protos.h"
#include "../src/machine/machine_protos.h"
#include "../src/cpu/nd500_mmu.h"
#include "../src/ndbus/ndbus_servicer.h"

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
     * TEST 7: PS -> PST -> capability table (declare_process_segment)
     *
     * The walk that reaches a capability starts at PS, an INDEX into the
     * physical segment table, not at a base address. ND-05.020.01 section 6.6:
     * "This register points to an element of the Physical Segment Table. The PST
     * element addresses the process segment of the process." Chapter 11's PSCAPT
     * nanostate refuses double indexing for a process segment outright, so the
     * three cases below are DIRECT resolves, SINGLE resolves through one index
     * page, and DOUBLE is an error.
     *
     * Unlike the tests above, this one counts its failures and makes the program
     * exit non-zero, so ctest can actually see it fail.
     * --------------------------------------------------------- */
    printf("Test 7: PS -> PST -> capability table\n");
    printf("-------------------------------------\n");

    int t7_failed = 0;
#define T7_CHECK(cond, what)                                                        \
    do {                                                                            \
        if (!(cond)) { printf("  FAIL: %s\n", (what)); t7_failed++; }                \
        else         { printf("  ok:   %s\n", (what)); }                            \
    } while (0)

    {
        /* A PST inside the 1 MB test machine, and a process segment at page 0x40. */
        const uint32_t pstp     = 0x20000u;
        const uint32_t ps        = 3u;
        const uint32_t seg_page  = 0x40u;          /* 0x40 << 11 = 0x20000... */
        const uint32_t cell      = pstp + ps * 4u;

        cpu.PSTP = pstp;

        /* A zero entry is "no process segment", not entry zero of the table. */
        for (uint32_t i = 0; i < 4u; i++) { nd500_bus_write8(&machine, cell + i, 0x00); }
        cpu.dit_configured = 0;
        T7_CHECK(nd500_mmu_declare_process_segment(&cpu, ps, NULL) == -1,
                 "a zero PST entry is refused");
        T7_CHECK(cpu.dit_configured == 0, "and nothing is declared");

        /* PS itself must not be zero. */
        T7_CHECK(nd500_mmu_declare_process_segment(&cpu, 0u, NULL) == -1,
                 "PS zero is refused");

        /* DIRECT (mode 0): the entry's page IS the process segment. */
        uint32_t direct = ((uint32_t)PS_AZI << 30) | 0x0123u;
        for (uint32_t i = 0; i < 4u; i++) {
            nd500_bus_write8(&machine, cell + i, (uint8_t)(direct >> (24u - i * 8u)));
        }
        uint32_t base = 0xDEADBEEFu;
        T7_CHECK(nd500_mmu_declare_process_segment(&cpu, ps, &base) == 0,
                 "a DIRECT entry resolves");
        T7_CHECK(base == (0x0123u << PGSHIFT), "to its own page, shifted to bytes");
        T7_CHECK(cpu.DITBASE == base && cpu.dit_configured == 1,
                 "and the capability table base is declared");

        /* SINGLE (mode 1): the entry's page is an index page; entry 0 of it names
         * the process segment. A PTE is 4 bytes; bit 0 of the low byte is the
         * valid bit as nd500_mmu_read_pte reads it, so build it through the
         * emulator's own writer rather than by hand. */
        uint32_t single = ((uint32_t)PS_ASI << 30) | seg_page;
        for (uint32_t i = 0; i < 4u; i++) {
            nd500_bus_write8(&machine, cell + i, (uint8_t)(single >> (24u - i * 8u)));
        }
        /* An INVALID index-page entry must be refused, not resolved to page 0. */
        for (uint32_t i = 0; i < 4u; i++) {
            nd500_bus_write8(&machine, (seg_page << PGSHIFT) + i, 0x00);
        }
        cpu.dit_configured = 0;
        T7_CHECK(nd500_mmu_declare_process_segment(&cpu, ps, NULL) == -1,
                 "a SINGLE entry whose index page is not valid is refused");

        /* DOUBLE (mode 2) is not allowed for a process segment. */
        uint32_t dbl = ((uint32_t)PS_ADI << 30) | seg_page;
        for (uint32_t i = 0; i < 4u; i++) {
            nd500_bus_write8(&machine, cell + i, (uint8_t)(dbl >> (24u - i * 8u)));
        }
        cpu.dit_configured = 0;
        T7_CHECK(nd500_mmu_declare_process_segment(&cpu, ps, NULL) == -1,
                 "DOUBLE indexing is refused - PSCAPT does not allow it here");
        T7_CHECK(cpu.dit_configured == 0, "and nothing is declared for it");

        /* No PSTP at all: nothing to index. */
        cpu.PSTP = 0;
        T7_CHECK(nd500_mmu_declare_process_segment(&cpu, ps, NULL) == -1,
                 "with PSTP unset there is no table to index");
    }
    printf("Status: %s\n\n", (t7_failed == 0) ? "PASS" : "FAIL");
#undef T7_CHECK

    /* ---------------------------------------------------------
     * TEST 8: the ND-100 operand mapping used by RIOM
     *
     * The ND-100-side operand of RIOM is TRANSPORT-SPECIFIC: SINTRAN's CNVWADR emits a
     * physical WORD address on the ND-500 3022 and a BYTE offset inside the 5MPM window
     * on the ND-5000 octobus. Getting it wrong does not fault - it reads memory that is
     * backed but was never written, so the transfer silently delivers zeros.
     *
     * Every number below is MEASURED, not chosen:
     *   3022    : source 0x00210718 must map to 0x00000E30, the 5MPM message buffer.
     *             Left at window_base 0 it maps to 0x00420E30, 4 MB into an 8 MB window.
     *   octobus : source 0x00008E30 must map to 0x00008E30 - the operand is ALREADY
     *             window-relative. Read as a word address it came out empty, the swapper
     *             scanned a record that had never been filled, and SINTRAN printed
     *             "Fatal error from Swapper", ERROR CODE 201B.
     * --------------------------------------------------------- */
    int t8_failed = 0;
#define T8_CHECK(cond, what)                                                        \
    do {                                                                            \
        if (!(cond)) { printf("  FAIL: %s\n", (what)); t8_failed++; }                \
        else { printf("  ok: %s\n", (what)); }                                      \
    } while (0)

    printf("Test 8: ND-100 operand mapping (RIOM)\n");
    printf("-------------------------------------\n");
    {
        Nd500Cpu m;
        memset(&m, 0, sizeof(m));
        nd500_cpu_init(&m, NULL);

        /* THE UNCONFIGURED STATE IS THE 3022 CONVENTION, and that is exactly why it must
         * be reported separately: an embedding that forgets to wire the mapping does not
         * fail, it silently behaves like a 3022. */
        T8_CHECK(m.nd100_mapping_configured == 0,
                 "a fresh CPU reports its mapping as NOT configured");
        T8_CHECK(m.nd100_bytes_per_unit == 2u,
                 "and the defaults are the 3022 word-address convention");

        /* --- the octobus reading, which is what the ND-5000 needs --- */
        T8_CHECK(nd500_cpu_set_nd100_mapping(&m, 0u, 1u, 0) == 0,
                 "the octobus mapping is accepted: base 0, 1 byte per unit");
        T8_CHECK(m.nd100_mapping_configured == 1,
                 "and the CPU now reports it as configured");
        T8_CHECK(nd500_cpu_map_nd100_to_physical(&m, 0x00008E30u) == 0x00008E30u,
                 "HSWPI 0x8E30 maps to window byte 0x8E30 - already window-relative");
        T8_CHECK(nd500_cpu_nd100_step_per_halfword(&m) == 2u,
                 "and one halfword advances the operand by 2 window bytes");

        /* --- the 3022 reading, which must keep working --- */
        T8_CHECK(nd500_cpu_set_nd100_mapping(&m, 0u, 2u, 0x420000) == 0,
                 "the 3022 mapping is accepted: 2 bytes per unit, window base 0x420000");
        T8_CHECK(nd500_cpu_map_nd100_to_physical(&m, 0x00210718u) == 0x00000E30u,
                 "word address 0x210718 maps to 0x00000E30, the 5MPM message buffer");
        T8_CHECK(nd500_cpu_nd100_step_per_halfword(&m) == 1u,
                 "and one halfword advances the operand by 1 word");

        /* The window base is not decoration: without it the same source lands 4 MB away,
         * in memory that is backed and empty - the original zero-transfer. */
        (void)nd500_cpu_set_nd100_mapping(&m, 0u, 2u, 0);
        T8_CHECK(nd500_cpu_map_nd100_to_physical(&m, 0x00210718u) == 0x00420E30u,
                 "dropping the window base moves it to 0x00420E30 - the measured defect");

        /* A scale that is not an ND address convention must fail loudly rather than
         * quietly scale by 4, and must leave the previous mapping untouched. */
        T8_CHECK(nd500_cpu_set_nd100_mapping(&m, 0u, 4u, 0) == -1,
                 "a scale of 4 is refused - it is not an ND address convention");
        T8_CHECK(m.nd100_bytes_per_unit == 2u,
                 "and the refused call changed nothing");
        T8_CHECK(nd500_cpu_set_nd100_mapping(&m, 0u, 0u, 0) == -1,
                 "a scale of 0 is refused too - it would divide by zero");
        T8_CHECK(nd500_cpu_set_nd100_mapping(NULL, 0u, 1u, 0) == -1,
                 "and a NULL CPU is refused");

        /* The servicer is the one source of truth for the octobus scale; RIOM must not
         * restate it. If this ever disagrees, the copy engine and RIOM have drifted. */
        T8_CHECK(ndbus_servicer_nd100_bytes_per_unit() == 1u,
                 "the servicer reports the octobus byte-offset convention");
    }
    printf("Status: %s\n\n", (t8_failed == 0) ? "PASS" : "FAIL");
#undef T8_CHECK

    /* ---------------------------------------------------------
     * SUMMARY
     * --------------------------------------------------------- */
    printf("===========================================\n");
    printf("OK All MMU unit tests complete!\n");
    printf("===========================================\n");
    printf("\nNote: Full translation tests (PS_ASI/PS_ADI) require\n");
    printf("      integration with memory bus and will be tested\n");
    printf("      during Phase 5 (Memory Bus Integration).\n");

    if (t7_failed != 0 || t8_failed != 0) {
        printf("\n%d check(s) FAILED in Test 7, %d in Test 8\n", t7_failed, t8_failed);
        return 1;
    }
    return 0;
}
