#include "nd500_mmu.h"
#include "cpu_protos.h"
#include "../machine/machine_protos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*
 * ND-500 MMU Implementation
 * Based on C# RetroCore emulator CpuND500.MMU.cs
 */

// ═══════════════════════════════════════════════════════
// MMU STATE (stored in CPU structure - to be added in Phase 3)
// ═══════════════════════════════════════════════════════

// For now, we'll use static storage. In Phase 3, this will be integrated
// into the Nd500Cpu structure.
static PhysicalSegmentTableEntry* g_pst = NULL;
static ProcessControlBlock* g_pcb_table = NULL;
static int g_mmu_enabled = 0;

// ═══════════════════════════════════════════════════════
// MMU INITIALIZATION
// ═══════════════════════════════════════════════════════

void nd500_mmu_init(Nd500Cpu* cpu) {
    if (!cpu) return;

    /* Allocate PST (8192 entries * 8 bytes each) */
    if (!g_pst) {
        g_pst = (PhysicalSegmentTableEntry*)calloc(MAX_PST, sizeof(PhysicalSegmentTableEntry));
        if (!g_pst) {
            fprintf(stderr, "ND-500: Failed to allocate PST (%d entries)\n", MAX_PST);
            return;
        }
    }

    /* Allocate PCB table (256 domains * PCB size) */
    if (!g_pcb_table) {
        g_pcb_table = (ProcessControlBlock*)calloc(MAXDOM, sizeof(ProcessControlBlock));
        if (!g_pcb_table) {
            fprintf(stderr, "ND-500: Failed to allocate PCB table (%d domains)\n", MAXDOM);
            return;
        }
    }

    /* MMU starts disabled */
    g_mmu_enabled = 0;

    printf("ND-500: MMU initialized - PST: %d entries, PCB: %d domains\n", MAX_PST, MAXDOM);
}

void nd500_mmu_enable(Nd500Cpu* cpu) {
    if (!cpu) return;
    g_mmu_enabled = 1;
    printf("ND-500: MMU enabled\n");
}

void nd500_mmu_disable(Nd500Cpu* cpu) {
    if (!cpu) return;
    g_mmu_enabled = 0;
    printf("ND-500: MMU disabled\n");
}

int nd500_mmu_is_enabled(Nd500Cpu* cpu) {
    return g_mmu_enabled;
}

// ═══════════════════════════════════════════════════════
// MMU ADDRESS TRANSLATION
// ═══════════════════════════════════════════════════════

/**
 * Translate virtual address to physical address
 * This is a stub implementation - will be completed in Phase 3
 */
uint32_t nd500_mmu_translate(Nd500Cpu* cpu, uint32_t virtual_addr, int is_write, int is_instruction) {
    /* If MMU disabled, direct mapping */
    if (!g_mmu_enabled || !cpu) {
        return virtual_addr;
    }

    /* TODO (Phase 3): Implement three-level translation:
     * 1. Extract segment, page, offset from virtual address
     * 2. Get capability from PCB[domain].capabilities[segment]
     * 3. Extract PSN from capability
     * 4. Get PST entry and translate based on mode (AZI/ASI/ADI)
     * 5. Return physical address
     */

    /* For now, return direct mapping */
    return virtual_addr;
}

/**
 * phyladr - Public wrapper for address translation
 * Used by debugger and system utilities
 */
uint32_t nd500_mmu_phyladr(Nd500Cpu* cpu, uint32_t virtual_addr) {
    /* Use translate with read access, not instruction fetch */
    return nd500_mmu_translate(cpu, virtual_addr, 0, 0);
}

// ═══════════════════════════════════════════════════════
// PST ACCESSORS
// ═══════════════════════════════════════════════════════

PhysicalSegmentTableEntry nd500_mmu_get_pst_entry(Nd500Cpu* cpu, int psn) {
    PhysicalSegmentTableEntry empty = {0, 0};

    if (!g_pst || psn < 0 || psn >= MAX_PST) {
        return empty;
    }

    return g_pst[psn];
}

void nd500_mmu_set_pst_entry(Nd500Cpu* cpu, int psn, uint8_t index_mode, uint32_t pfn) {
    if (!g_pst || psn < 0 || psn >= MAX_PST) {
        return;
    }

    g_pst[psn].index_mode = index_mode;
    g_pst[psn].physical_pfn = pfn & 0x3FFFFFFF;  /* 30 bits */
}

// ═══════════════════════════════════════════════════════
// PCB ACCESSORS
// ═══════════════════════════════════════════════════════

ProcessControlBlock* nd500_mmu_get_pcb(Nd500Cpu* cpu, uint8_t domain) {
    if (!g_pcb_table || domain >= MAXDOM) {
        return NULL;
    }

    return &g_pcb_table[domain];
}

uint16_t nd500_mmu_get_program_capability(Nd500Cpu* cpu, uint8_t domain, int segment) {
    if (!g_pcb_table || domain >= MAXDOM || segment < 0 || segment >= MAXSEG) {
        return 0;
    }

    return g_pcb_table[domain].program_capabilities[segment];
}

uint16_t nd500_mmu_get_data_capability(Nd500Cpu* cpu, uint8_t domain, int segment) {
    if (!g_pcb_table || domain >= MAXDOM || segment < 0 || segment >= MAXSEG) {
        return 0;
    }

    return g_pcb_table[domain].data_capabilities[segment];
}

void nd500_mmu_set_program_capability(Nd500Cpu* cpu, uint8_t domain, int segment, uint16_t capability) {
    if (!g_pcb_table || domain >= MAXDOM || segment < 0 || segment >= MAXSEG) {
        return;
    }

    g_pcb_table[domain].program_capabilities[segment] = capability;
}

void nd500_mmu_set_data_capability(Nd500Cpu* cpu, uint8_t domain, int segment, uint16_t capability) {
    if (!g_pcb_table || domain >= MAXDOM || segment < 0 || segment >= MAXSEG) {
        return;
    }

    g_pcb_table[domain].data_capabilities[segment] = capability;
}

// ═══════════════════════════════════════════════════════
// PTE READ/WRITE (from physical memory)
// ═══════════════════════════════════════════════════════

/**
 * Read a Page Table Entry from physical memory
 * PTE is 4 bytes: [31:2]=PFN, [1]=unused, [0]=protection
 */
PageTableEntry nd500_mmu_read_pte(Nd500Cpu* cpu, uint32_t physical_addr) {
    PageTableEntry pte = {0, 0};

    if (!cpu || !cpu->machine) {
        return pte;
    }

    /* Read 4 bytes from physical memory (little endian) */
    uint8_t b0 = nd500_bus_read8(cpu->machine, physical_addr);
    uint8_t b1 = nd500_bus_read8(cpu->machine, physical_addr + 1);
    uint8_t b2 = nd500_bus_read8(cpu->machine, physical_addr + 2);
    uint8_t b3 = nd500_bus_read8(cpu->machine, physical_addr + 3);

    uint32_t pte_value = (uint32_t)(b0 | (b1 << 8) | (b2 << 16) | (b3 << 24));

    pte.protection = (uint8_t)(pte_value & 0x1);
    pte.physical_pfn = (pte_value >> 2) & 0x3FFFFFFF;

    return pte;
}

/**
 * Write a Page Table Entry to physical memory
 */
void nd500_mmu_write_pte(Nd500Cpu* cpu, uint32_t physical_addr, PageTableEntry pte) {
    if (!cpu || !cpu->machine) {
        return;
    }

    uint32_t pte_value = ((pte.physical_pfn & 0x3FFFFFFF) << 2) | (uint32_t)pte.protection;

    nd500_bus_write8(cpu->machine, physical_addr, (uint8_t)(pte_value & 0xFF));
    nd500_bus_write8(cpu->machine, physical_addr + 1, (uint8_t)((pte_value >> 8) & 0xFF));
    nd500_bus_write8(cpu->machine, physical_addr + 2, (uint8_t)((pte_value >> 16) & 0xFF));
    nd500_bus_write8(cpu->machine, physical_addr + 3, (uint8_t)((pte_value >> 24) & 0xFF));
}

// ═══════════════════════════════════════════════════════
// CACHE CONTROL
// ═══════════════════════════════════════════════════════

/**
 * DCTSB - Data Cache TSB Clear
 * Clear data cache Translation Storage Buffer
 * Must be called after modifying PTEs, PST, or PCB capabilities
 */
void nd500_mmu_clear_data_cache_tsb(Nd500Cpu* cpu) {
    /* In a full implementation, this would clear TLB entries */
    /* For now, it's a no-op since we don't cache translations */
    if (cpu) {
        printf("ND-500: DCTSB - Data cache TSB cleared\n");
    }
}

/**
 * PCTSB - Program Cache TSB Clear
 * Clear program (instruction) cache Translation Storage Buffer
 * Must be called after modifying code
 */
void nd500_mmu_clear_program_cache_tsb(Nd500Cpu* cpu) {
    /* In a full implementation, this would clear instruction TLB */
    if (cpu) {
        printf("ND-500: PCTSB - Program cache TSB cleared\n");
    }
}
