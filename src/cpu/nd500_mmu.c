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

// Separate I&D (Instruction & Data) MMU enable flags
// The ND-500 has independent MMU control for instruction and data accesses
static int g_mmu_data_enabled = 0;     // Controlled by DMON/DMOF instructions
static int g_mmu_program_enabled = 0;  // Controlled by PMON/PMOF instructions

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

    /* MMU starts disabled (both data and program) */
    g_mmu_data_enabled = 0;
    g_mmu_program_enabled = 0;

    printf("ND-500: MMU initialized - PST: %d entries, PCB: %d domains\n", MAX_PST, MAXDOM);
}

// ═══════════════════════════════════════════════════════
// DATA MMU CONTROL (DMON/DMOF instructions)
// ═══════════════════════════════════════════════════════

void nd500_mmu_enable_data(Nd500Cpu* cpu) {
    if (!cpu) return;
    g_mmu_data_enabled = 1;
    printf("ND-500: Data MMU enabled (DMON)\n");
}

void nd500_mmu_disable_data(Nd500Cpu* cpu) {
    if (!cpu) return;
    g_mmu_data_enabled = 0;
    printf("ND-500: Data MMU disabled (DMOF)\n");
}

int nd500_mmu_is_data_enabled(Nd500Cpu* cpu) {
    return g_mmu_data_enabled;
}

// ═══════════════════════════════════════════════════════
// PROGRAM MMU CONTROL (PMON/PMOF instructions)
// ═══════════════════════════════════════════════════════

void nd500_mmu_enable_program(Nd500Cpu* cpu) {
    if (!cpu) return;
    g_mmu_program_enabled = 1;
    printf("ND-500: Program MMU enabled (PMON)\n");
}

void nd500_mmu_disable_program(Nd500Cpu* cpu) {
    if (!cpu) return;
    g_mmu_program_enabled = 0;
    printf("ND-500: Program MMU disabled (PMOF)\n");
}

int nd500_mmu_is_program_enabled(Nd500Cpu* cpu) {
    return g_mmu_program_enabled;
}

// ═══════════════════════════════════════════════════════
// LEGACY FUNCTIONS (for compatibility)
// ═══════════════════════════════════════════════════════

void nd500_mmu_enable(Nd500Cpu* cpu) {
    /* Enable BOTH data and program MMU (legacy behavior) */
    nd500_mmu_enable_data(cpu);
    nd500_mmu_enable_program(cpu);
}

void nd500_mmu_disable(Nd500Cpu* cpu) {
    /* Disable BOTH data and program MMU (legacy behavior) */
    nd500_mmu_disable_data(cpu);
    nd500_mmu_disable_program(cpu);
}

int nd500_mmu_is_enabled(Nd500Cpu* cpu) {
    /* Return true if EITHER MMU is enabled (legacy behavior) */
    return g_mmu_data_enabled || g_mmu_program_enabled;
}

// ═══════════════════════════════════════════════════════
// MMU ADDRESS TRANSLATION
// ═══════════════════════════════════════════════════════

/**
 * Translate virtual address to physical address
 * Implements three-level address translation:
 *   1. Virtual Address → Capability (via PCB)
 *   2. Capability → PST Entry (via PSN)
 *   3. PST Entry → Physical Page (AZI/ASI/ADI modes)
 *
 * Based on C# CpuND500.MMU.cs TranslateVirtualAddress() (lines 282-448)
 */
uint32_t nd500_mmu_translate(Nd500Cpu* cpu, uint32_t virtual_addr, int is_write, int is_instruction) {
    if (!cpu) {
        return virtual_addr;
    }

    /* Check if appropriate MMU is enabled based on access type */
    if (is_instruction) {
        /* Instruction fetch: check program MMU */
        if (!g_mmu_program_enabled) {
            return virtual_addr;  /* Program MMU disabled - direct physical addressing */
        }
    } else {
        /* Data access: check data MMU */
        if (!g_mmu_data_enabled) {
            return virtual_addr;  /* Data MMU disabled - direct physical addressing */
        }
    }

    /* Sanity check tables */
    if (!g_pst || !g_pcb_table) {
        return virtual_addr;  /* MMU not initialized */
    }

    /* ─────────────────────────────────────────────────────────
     * LEVEL 1: Virtual Address → Capability
     * ───────────────────────────────────────────────────────── */

    /* Extract address components: [Segment(5) | Page(16) | Offset(11)] */
    int segment = (virtual_addr >> 27) & 0x1F;         /* Bits 31-27 */
    int page = (virtual_addr >> PGSHIFT) & 0xFFFF;     /* Bits 26-11 */
    int offset = virtual_addr & (NBPG - 1);            /* Bits 10-0 */

    /* Get current domain (CAD = Current Alternative Domain) */
    uint8_t domain = (uint8_t)cpu->CAD;
    /* Note: domain is uint8_t (0-255), MAXDOM is 256, so domain < MAXDOM is always true */

    /* Get capability from PCB */
    uint16_t capability;
    if (is_instruction) {
        /* Instruction fetch: use program capability */
        capability = g_pcb_table[domain].program_capabilities[segment];
    } else {
        /* Data access: use data capability */
        capability = g_pcb_table[domain].data_capabilities[segment];
    }

    /* Check if capability is valid (non-zero) */
    if (capability == 0) {
        trap_protect_violation(cpu, cpu->PC, virtual_addr);
        return 0;  /* No access rights to this segment */
    }

    /* ─────────────────────────────────────────────────────────
     * LEVEL 2: Capability → PST Entry
     * ───────────────────────────────────────────────────────── */

    /* Extract PSN (Physical Segment Number) from capability */
    int psn = capability & PC_PSN;  /* Lower 13 bits */

    if (psn >= MAX_PST) {
        trap_protect_violation(cpu, cpu->PC, virtual_addr);
        return 0;  /* Invalid PSN */
    }

    /* Check write permission (for data writes only) */
    if (!is_instruction && is_write) {
        /* Check DC_WRP flag: 0=writable, 1=read-only */
        if (capability & DC_WRP) {
            trap_protect_violation(cpu, cpu->PC, virtual_addr);
            return 0;  /* Write to read-only segment */
        }
    }

    /* Get PST entry */
    PhysicalSegmentTableEntry pst_entry = g_pst[psn];

    /* ─────────────────────────────────────────────────────────
     * LEVEL 3: PST Entry → Physical Address
     * Mode-dependent translation (AZI, ASI, ADI)
     * ───────────────────────────────────────────────────────── */

    uint32_t physical_pfn;

    switch (pst_entry.index_mode) {
        case PS_AZI: {
            /* Mode 0: Direct Addressing (no paging) */
            /* Physical PFN comes directly from PST entry */
            physical_pfn = pst_entry.physical_pfn;
            break;
        }

        case PS_ASI: {
            /* Mode 1: Single-Level Paging */
            /* PST entry points to a page table */
            uint32_t page_table_base = pst_entry.physical_pfn << PGSHIFT;
            uint32_t pte_addr = page_table_base + (page * 4);  /* 4 bytes per PTE */

            /* Read PTE from memory */
            PageTableEntry pte = nd500_mmu_read_pte(cpu, pte_addr);

            /* Check if page is present */
            if (pte.physical_pfn == 0) {
                trap_page_fault(cpu, cpu->PC, virtual_addr);
                return 0;  /* Page not mapped */
            }

            /* Check write permission */
            if (is_write && pte.protection != 0) {
                trap_protect_violation(cpu, cpu->PC, virtual_addr);
                return 0;  /* Write to read-only page */
            }

            physical_pfn = pte.physical_pfn;
            break;
        }

        case PS_ADI: {
            /* Mode 2: Two-Level Paging */
            /* PST entry points to L1 page table */
            uint32_t l1_table_base = pst_entry.physical_pfn << PGSHIFT;

            /* Extract L1 and L2 indices from page number */
            int l1_index = (page >> 8) & 0xFF;   /* Upper 8 bits of page */
            int l2_index = page & 0xFF;          /* Lower 8 bits of page */

            /* Read L1 PTE */
            uint32_t l1_pte_addr = l1_table_base + (l1_index * 4);
            PageTableEntry l1_pte = nd500_mmu_read_pte(cpu, l1_pte_addr);

            if (l1_pte.physical_pfn == 0) {
                trap_page_fault(cpu, cpu->PC, virtual_addr);
                return 0;  /* L1 page table not present */
            }

            /* L1 PTE points to L2 page table */
            uint32_t l2_table_base = l1_pte.physical_pfn << PGSHIFT;
            uint32_t l2_pte_addr = l2_table_base + (l2_index * 4);

            /* Read L2 PTE */
            PageTableEntry l2_pte = nd500_mmu_read_pte(cpu, l2_pte_addr);

            if (l2_pte.physical_pfn == 0) {
                trap_page_fault(cpu, cpu->PC, virtual_addr);
                return 0;  /* L2 page not mapped */
            }

            /* Check write permission */
            if (is_write && (l1_pte.protection != 0 || l2_pte.protection != 0)) {
                trap_protect_violation(cpu, cpu->PC, virtual_addr);
                return 0;  /* Write to read-only page */
            }

            physical_pfn = l2_pte.physical_pfn;
            break;
        }

        default:
            /* Invalid index mode */
            trap_illegal_operand(cpu, cpu->PC);
            return 0;
    }

    /* ─────────────────────────────────────────────────────────
     * Construct physical address: (PFN << 11) | Offset
     * ───────────────────────────────────────────────────────── */

    uint32_t physical_addr = (physical_pfn << PGSHIFT) | offset;

    return physical_addr;
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
    if (!g_pcb_table) {
        return NULL;
    }
    /* Note: domain is uint8_t (0-255), MAXDOM is 256, range check not needed */
    return &g_pcb_table[domain];
}

uint16_t nd500_mmu_get_program_capability(Nd500Cpu* cpu, uint8_t domain, int segment) {
    if (!g_pcb_table || segment < 0 || segment >= MAXSEG) {
        return 0;
    }
    /* Note: domain is uint8_t (0-255), MAXDOM is 256, range check not needed */
    return g_pcb_table[domain].program_capabilities[segment];
}

uint16_t nd500_mmu_get_data_capability(Nd500Cpu* cpu, uint8_t domain, int segment) {
    if (!g_pcb_table || segment < 0 || segment >= MAXSEG) {
        return 0;
    }
    /* Note: domain is uint8_t (0-255), MAXDOM is 256, range check not needed */
    return g_pcb_table[domain].data_capabilities[segment];
}

void nd500_mmu_set_program_capability(Nd500Cpu* cpu, uint8_t domain, int segment, uint16_t capability) {
    if (!g_pcb_table || segment < 0 || segment >= MAXSEG) {
        return;
    }
    /* Note: domain is uint8_t (0-255), MAXDOM is 256, range check not needed */
    g_pcb_table[domain].program_capabilities[segment] = capability;
}

void nd500_mmu_set_data_capability(Nd500Cpu* cpu, uint8_t domain, int segment, uint16_t capability) {
    if (!g_pcb_table || segment < 0 || segment >= MAXSEG) {
        return;
    }
    /* Note: domain is uint8_t (0-255), MAXDOM is 256, range check not needed */
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
