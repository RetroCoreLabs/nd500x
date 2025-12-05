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

/* Ensure MMU tables are allocated (lazy initialization) */
static void ensure_mmu_tables(void) {
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
}

void nd500_mmu_init(Nd500Cpu* cpu) {
    if (!cpu) return;

    ensure_mmu_tables();

    /* MMU starts disabled (both data and program) */
    g_mmu_data_enabled = 0;
    g_mmu_program_enabled = 0;
}

// ═══════════════════════════════════════════════════════
// DATA MMU CONTROL (DMON/DMOF instructions)
// ═══════════════════════════════════════════════════════

void nd500_mmu_enable_data(Nd500Cpu* cpu) {
    if (!cpu) return;
    g_mmu_data_enabled = 1;
    /* Also set machine->mmu_enabled so data access uses MMU translation */
    if (cpu->machine) cpu->machine->mmu_enabled = 1;
    printf("ND-500: Data MMU enabled (DMON)\n");
}

void nd500_mmu_disable_data(Nd500Cpu* cpu) {
    if (!cpu) return;
    g_mmu_data_enabled = 0;
    /* Disable machine mmu_enabled only if both program AND data MMU are disabled */
    if (cpu->machine && !g_mmu_program_enabled) cpu->machine->mmu_enabled = 0;
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
    /* Also set machine->mmu_enabled so instruction decode uses MMU translation */
    if (cpu->machine) cpu->machine->mmu_enabled = 1;
    printf("ND-500: Program MMU enabled (PMON)\n");
}

void nd500_mmu_disable_program(Nd500Cpu* cpu) {
    if (!cpu) return;
    g_mmu_program_enabled = 0;
    /* Disable machine mmu_enabled only if both program AND data MMU are disabled */
    if (cpu->machine && !g_mmu_data_enabled) cpu->machine->mmu_enabled = 0;
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
    /* Debug: trace all translations for high addresses */
    if (virtual_addr >= 0x08000000 && is_write) {
        fprintf(stderr, "[MMU-TRACE] translate(vaddr=0x%08X, is_write=%d, is_instr=%d)\n",
                virtual_addr, is_write, is_instruction);
    }

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
            /* Debug: warn when data MMU is disabled but we're trying to translate */
            if (virtual_addr >= 0x08000000) {
                fprintf(stderr, "[MMU] Data MMU DISABLED! vaddr=0x%08X returned unchanged (DMON not executed?)\n", virtual_addr);
            }
            return virtual_addr;  /* Data MMU disabled - direct physical addressing */
        }
    }

    /* Sanity check tables */
    if (!g_pst || !g_pcb_table) {
        fprintf(stderr, "[MMU] Tables not initialized! PST=%p PCB=%p\n", (void*)g_pst, (void*)g_pcb_table);
        return virtual_addr;  /* MMU not initialized */
    }

    /* ─────────────────────────────────────────────────────────
     * LEVEL 1: Virtual Address → Capability
     * ───────────────────────────────────────────────────────── */

    /* Extract address components per ND-500 architecture (ND-05.009.4, p53-54):
     * [Segment(5) | L1 Index(7) | L2 Index(9) | Offset(11)] */
    int segment  = (virtual_addr >> SGSHIFT) & 0x1F;                      /* Bits 31-27 */
    int l1_index = (virtual_addr >> L1_INDEX_SHIFT) & L1_INDEX_MASK;      /* Bits 26-20 */
    int l2_index = (virtual_addr >> L2_INDEX_SHIFT) & L2_INDEX_MASK;      /* Bits 19-11 */
    int offset   = virtual_addr & (NBPG - 1);                             /* Bits 10-0 */

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
        fprintf(stderr, "[MMU] No data capability for domain=%d segment=%d (vaddr=0x%08X)\n",
                domain, segment, virtual_addr);
        trap_protect_violation(cpu, cpu->PC, virtual_addr);
        return virtual_addr;  /* Return virtual address, trap will stop execution */
    }

    /* ─────────────────────────────────────────────────────────
     * LEVEL 2: Capability → PST Entry
     * ───────────────────────────────────────────────────────── */

    /* Extract PSN (Physical Segment Number) from capability */
    int psn = capability & PC_PSN;  /* Lower 13 bits */

    if (psn >= MAX_PST) {
        fprintf(stderr, "[MMU] PSN %d >= MAX_PST %d! vaddr=0x%08X\n", psn, MAX_PST, virtual_addr);
        trap_protect_violation(cpu, cpu->PC, virtual_addr);
        return virtual_addr;  /* Invalid PSN - return virtual address, trap will stop execution */
    }

    /* Check write permission (for data writes only) */
    if (!is_instruction && is_write) {
        /* Check DC_WRP flag: DC_WRP SET = Write Permitted, DC_WRP CLEAR = Read-only */
        if (!(capability & DC_WRP)) {
            fprintf(stderr, "[MMU] WRITE DENIED! segment=%d missing DC_WRP (write-permit) flag! capability=0x%04X vaddr=0x%08X\n",
                    segment, capability, virtual_addr);
            trap_protect_violation(cpu, cpu->PC, virtual_addr);
            return virtual_addr;  /* Write to read-only segment - return virtual address, trap will stop execution */
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
            /* Mode 0: Direct Addressing (no paging) - single 2KB page only */
            /* For PS_AZI, both L1 and L2 indices must be 0 */
            if (l1_index != 0 || l2_index != 0) {
                fprintf(stderr, "[MMU] PS_AZI: L1=%d L2=%d must be 0! vaddr=0x%08X\n",
                        l1_index, l2_index, virtual_addr);
                trap_page_fault(cpu, cpu->PC, virtual_addr);
                return virtual_addr;
            }
            /* Physical PFN comes directly from PST entry */
            physical_pfn = pst_entry.physical_pfn;
            break;
        }

        case PS_ASI: {
            /* Mode 1: Single-Level Paging (up to 512 pages = 1MB) */
            /* For PS_ASI, L1 must be 0; L2 selects page table entry */
            if (l1_index != 0) {
                fprintf(stderr, "[MMU] PS_ASI: L1=%d must be 0! vaddr=0x%08X\n",
                        l1_index, virtual_addr);
                trap_page_fault(cpu, cpu->PC, virtual_addr);
                return virtual_addr;
            }
            /* PST entry points to a page table */
            uint32_t page_table_base = pst_entry.physical_pfn << PGSHIFT;
            uint32_t pte_addr = page_table_base + (l2_index * 4);  /* Use L2 index */

            /* Read PTE from memory */
            PageTableEntry pte = nd500_mmu_read_pte(cpu, pte_addr);

            /* Check if page is present (valid bit must be set) */
            if (!pte.valid) {
                fprintf(stderr, "[MMU] PS_ASI: PTE not valid! vaddr=0x%08X pte_addr=0x%08X\n", virtual_addr, pte_addr);
                trap_page_fault(cpu, cpu->PC, virtual_addr);
                return virtual_addr;  /* Page not mapped - return virtual address, trap will stop execution */
            }

            /* PTE protection check - only applies to INSTRUCTION fetch, not data access.
             * For data writes, permission is controlled by DC_WRP capability flag (already checked above).
             * C# reference creates all PTEs with protection=1, data writes work via DC_WRP. */
            if (is_instruction && is_write && pte.protection != 0) {
                fprintf(stderr, "[MMU] PS_ASI: Instruction write to read-only page! vaddr=0x%08X pte_addr=0x%08X prot=%d\n",
                        virtual_addr, pte_addr, pte.protection);
                trap_protect_violation(cpu, cpu->PC, virtual_addr);
                return virtual_addr;
            }

            physical_pfn = pte.physical_pfn;
            break;
        }

        case PS_ADI: {
            /* Mode 2: Two-Level Paging (up to 128*512 = 65536 pages = 128MB) */
            /* L1 selects L2 page table (0-127), L2 selects entry (0-511) */
            /* l1_index and l2_index already extracted correctly at top of function */
            uint32_t l1_table_base = pst_entry.physical_pfn << PGSHIFT;

            /* Read L1 PTE using l1_index */
            uint32_t l1_pte_addr = l1_table_base + (l1_index * 4);
            PageTableEntry l1_pte = nd500_mmu_read_pte(cpu, l1_pte_addr);

            if (!l1_pte.valid) {
                trap_page_fault(cpu, cpu->PC, virtual_addr);
                return virtual_addr;  /* L1 page table not present - return virtual address, trap will stop execution */
            }

            /* L1 PTE points to L2 page table */
            uint32_t l2_table_base = l1_pte.physical_pfn << PGSHIFT;
            uint32_t l2_pte_addr = l2_table_base + (l2_index * 4);

            /* Read L2 PTE */
            PageTableEntry l2_pte = nd500_mmu_read_pte(cpu, l2_pte_addr);

            if (!l2_pte.valid) {
                trap_page_fault(cpu, cpu->PC, virtual_addr);
                return virtual_addr;  /* L2 page not mapped - return virtual address, trap will stop execution */
            }

            /* Check write permission */
            if (is_write && (l1_pte.protection != 0 || l2_pte.protection != 0)) {
                trap_protect_violation(cpu, cpu->PC, virtual_addr);
                return virtual_addr;  /* Write to read-only page - return virtual address, trap will stop execution */
            }

            physical_pfn = l2_pte.physical_pfn;
            break;
        }

        default:
            /* Invalid index mode */
            trap_illegal_operand(cpu, cpu->PC);
            return virtual_addr;  /* Invalid index mode - return virtual address, trap will stop execution */
    }

    /* ─────────────────────────────────────────────────────────
     * Construct physical address: (PFN << 11) | Offset
     * ───────────────────────────────────────────────────────── */

    uint32_t physical_addr = (physical_pfn << PGSHIFT) | offset;

    /* Debug: always show translation for high addresses on write */
    if (virtual_addr >= 0x08000000) {
        fprintf(stderr, "[MMU] vaddr=0x%08X -> paddr=0x%08X (seg=%d L1=%d L2=%d cap=0x%04X psn=%d mode=%d pfn=0x%X)\n",
                virtual_addr, physical_addr, segment, l1_index, l2_index, capability, psn, pst_entry.index_mode, physical_pfn);
    }

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

    ensure_mmu_tables();
    if (!g_pst || psn < 0 || psn >= MAX_PST) {
        return empty;
    }

    return g_pst[psn];
}

void nd500_mmu_set_pst_entry(Nd500Cpu* cpu, int psn, uint8_t index_mode, uint32_t pfn) {
    ensure_mmu_tables();
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
    ensure_mmu_tables();
    if (!g_pcb_table) {
        return NULL;
    }
    /* Note: domain is uint8_t (0-255), MAXDOM is 256, range check not needed */
    return &g_pcb_table[domain];
}

uint16_t nd500_mmu_get_program_capability(Nd500Cpu* cpu, uint8_t domain, int segment) {
    ensure_mmu_tables();
    if (!g_pcb_table || segment < 0 || segment >= MAXSEG) {
        return 0;
    }
    /* Note: domain is uint8_t (0-255), MAXDOM is 256, range check not needed */
    return g_pcb_table[domain].program_capabilities[segment];
}

uint16_t nd500_mmu_get_data_capability(Nd500Cpu* cpu, uint8_t domain, int segment) {
    ensure_mmu_tables();
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
 * PTE is 4 bytes: [31:2]=PFN, [1]=valid/present, [0]=protection
 */
PageTableEntry nd500_mmu_read_pte(Nd500Cpu* cpu, uint32_t physical_addr) {
    PageTableEntry pte = {0, 0, 0};

    if (!cpu || !cpu->machine) {
        return pte;
    }

    /* Read 4 bytes from physical memory (big endian - ND-500 is big endian) */
    uint8_t b0 = nd500_bus_read8(cpu->machine, physical_addr);
    uint8_t b1 = nd500_bus_read8(cpu->machine, physical_addr + 1);
    uint8_t b2 = nd500_bus_read8(cpu->machine, physical_addr + 2);
    uint8_t b3 = nd500_bus_read8(cpu->machine, physical_addr + 3);

    uint32_t pte_value = (uint32_t)((b0 << 24) | (b1 << 16) | (b2 << 8) | b3);

    /* PTE format (matches C# CpuND500.MMU.cs):
     * [31:2] = Physical Page Frame Number (30 bits)
     * [1]    = unused
     * [0]    = Protection (0=RW, 1=RO)
     * Valid = (PFN != 0) - there is no separate valid bit
     */
    pte.protection = (uint8_t)(pte_value & 0x1);
    pte.physical_pfn = (pte_value >> 2) & 0x3FFFFFFF;
    pte.valid = (pte.physical_pfn != 0) ? 1 : 0;  /* Valid if PFN is non-zero */

    return pte;
}

/**
 * Write a Page Table Entry to physical memory
 * Format (matches C# CpuND500.MMU.cs):
 *   [31:2] = PFN (30 bits)
 *   [1]    = unused
 *   [0]    = protection (0=RW, 1=RO)
 * Written in big-endian (ND-500 native byte order)
 */
void nd500_mmu_write_pte(Nd500Cpu* cpu, uint32_t physical_addr, PageTableEntry pte) {
    if (!cpu || !cpu->machine) {
        return;
    }

    uint32_t pte_value = ((pte.physical_pfn & 0x3FFFFFFF) << 2) |
                         (uint32_t)pte.protection;

    /* Write big-endian */
    nd500_bus_write8(cpu->machine, physical_addr, (uint8_t)((pte_value >> 24) & 0xFF));
    nd500_bus_write8(cpu->machine, physical_addr + 1, (uint8_t)((pte_value >> 16) & 0xFF));
    nd500_bus_write8(cpu->machine, physical_addr + 2, (uint8_t)((pte_value >> 8) & 0xFF));
    nd500_bus_write8(cpu->machine, physical_addr + 3, (uint8_t)(pte_value & 0xFF));
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
