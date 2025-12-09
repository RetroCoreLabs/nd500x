/*
 * ND-500 Segment Allocation for MON Calls
 *
 * Implements segment allocation callback for MON library handlers.
 * Used by MON 422B GetScratchSegment to allocate physical memory,
 * create page tables, set up PST entries, and configure PCB capabilities.
 *
 * Reference: ndlib/ndlib_dom_loader.c for allocation pattern
 */

#include "nd500_mmu.h"
#include "cpu_protos.h"
#include "../machine/machine_protos.h"
#include <stdio.h>
#include <string.h>

/* SINTRAN error codes */
#define ERR_SUCCESS               0
#define ERR_NO_PHYS_MEM           247  /* 367 octal */
#define ERR_NO_PST_INDEX          248  /* 370 octal */
#define ERR_ILLEGAL_SEGMENT       249  /* 371 octal */
#define ERR_ILLEGAL_ADDRESS       250  /* 372 octal */

/**
 * Find the highest physical page frame number used in PST entries.
 * Returns the highest PFN found, or 0 if no entries are used.
 */
static uint32_t find_highest_used_pfn(Nd500Cpu* cpu) {
    uint32_t highest_pfn = 0;

    for (int psn = 0; psn < MAX_PST; psn++) {
        PhysicalSegmentTableEntry pst = nd500_mmu_get_pst_entry(cpu, psn);
        
        /* Skip empty entries */
        if (pst.index_mode == 0 && pst.physical_pfn == 0) {
            continue;
        }

        /* For PS_ASI mode, the PFN points to a page table.
         * We need to scan the page table to find the highest physical page.
         * For simplicity, we'll use the page table PFN itself as a lower bound.
         */
        if (pst.physical_pfn > highest_pfn) {
            highest_pfn = pst.physical_pfn;
        }

        /* If this is PS_ASI, scan the page table for actual page PFNs */
        if (pst.index_mode == PS_ASI) {
            uint32_t page_table_base = pst.physical_pfn << PGSHIFT;
            
            /* Scan page table entries (up to 512 entries per page table) */
            for (uint32_t i = 0; i < NPTEPG; i++) {
                uint32_t pte_addr = page_table_base + (i * 4);
                
                /* Read PTE */
                uint8_t b0 = nd500_bus_read8(cpu->machine, pte_addr);
                uint8_t b1 = nd500_bus_read8(cpu->machine, pte_addr + 1);
                uint8_t b2 = nd500_bus_read8(cpu->machine, pte_addr + 2);
                uint8_t b3 = nd500_bus_read8(cpu->machine, pte_addr + 3);
                uint32_t pte_value = (uint32_t)((b0 << 24) | (b1 << 16) | (b2 << 8) | b3);
                
                /* Extract PFN from PTE (bits 31:2) */
                uint32_t pte_pfn = (pte_value >> 2) & 0x3FFFFFFF;
                
                if (pte_pfn > 0 && pte_pfn > highest_pfn) {
                    highest_pfn = pte_pfn;
                }
            }
        }
    }

    return highest_pfn;
}

/**
 * Find a free PSN starting from start_psn.
 * Returns the first free PSN found, or -1 if none available.
 */
static int find_free_psn(Nd500Cpu* cpu, int start_psn) {
    for (int psn = start_psn; psn < MAX_PST; psn++) {
        PhysicalSegmentTableEntry pst = nd500_mmu_get_pst_entry(cpu, psn);
        
        /* Check if PSN is free */
        if (pst.index_mode == 0 && pst.physical_pfn == 0) {
            return psn;
        }
    }
    
    return -1;  /* No free PSN found */
}

/**
 * MON callback: Allocate a scratch segment
 *
 * Parameters:
 *   cpu                  - CPU pointer (opaque void*)
 *   machine              - Machine pointer (opaque void*)
 *   domain               - Domain number (from cpu->CED)
 *   requested_segment    - Requested segment number (0 = auto-assign)
 *   segment_size_bytes   - Size in bytes
 *   out_assigned_segment - Returns assigned segment number
 *
 * Returns: 0 on success, SINTRAN error code on failure
 */
int nd500_mon_allocate_segment(void* cpu_ptr, void* machine_ptr, uint8_t domain,
    uint32_t requested_segment, uint32_t segment_size_bytes,
    uint32_t* out_assigned_segment)
{
    if (!cpu_ptr || !machine_ptr || !out_assigned_segment) {
        return ERR_ILLEGAL_ADDRESS;
    }

    Nd500Cpu* cpu = (Nd500Cpu*)cpu_ptr;
    Nd500Machine* m = (Nd500Machine*)machine_ptr;

    /* Validate segment number */
    if (requested_segment > 31) {
        return ERR_ILLEGAL_SEGMENT;
    }

    /* Get current domain from CPU if sentinel value (0xFF) was passed */
    if (domain == 0xFF || domain >= MAXDOM) {
        domain = (uint8_t)cpu->CED;  /* Use current executing domain */
    }

    /* Determine segment number to use */
    uint32_t assigned_segment;
    if (requested_segment == 0) {
        /* Auto-assign: find first free segment */
        /* Start from segment 2 (after segments 0 and 1 used by DOM loader) */
        int found = 0;
        for (uint32_t seg = 2; seg < MAXSEG; seg++) {
            uint16_t dc = nd500_mmu_get_data_capability(cpu, domain, seg);
            if (dc == 0) {
                assigned_segment = seg;
                found = 1;
                break;
            }
        }
        if (!found) {
            return ERR_ILLEGAL_SEGMENT;  /* No free segments */
        }
    } else {
        assigned_segment = requested_segment;
        
        /* Check if segment is already allocated */
        uint16_t dc = nd500_mmu_get_data_capability(cpu, domain, assigned_segment);
        if (dc != 0) {
            return ERR_ILLEGAL_SEGMENT;  /* Segment already allocated */
        }
    }

    /* Validate size */
    if (segment_size_bytes == 0) {
        return ERR_ILLEGAL_ADDRESS;
    }

    /* Round size up to 2KB page boundary */
    uint32_t rounded_size = (segment_size_bytes + 2047) & ~2047u;
    
    /* Calculate number of pages needed */
    uint32_t num_pages = (rounded_size + 2047) / 2048;
    if (num_pages == 0) num_pages = 1;

    /* Find free physical memory */
    uint32_t highest_pfn = find_highest_used_pfn(cpu);
    
    /* Calculate next free physical address */
    /* Start from highest used PFN + 1, or use a safe starting point */
    uint32_t start_pfn = highest_pfn + 1;
    if (start_pfn < 1000) {  /* Ensure we're past DOM loader allocations */
        start_pfn = 1000;  /* Start after known allocations */
    }
    
    uint32_t phys_segment_base = start_pfn << PGSHIFT;
    uint32_t phys_segment_end = phys_segment_base + rounded_size;
    
    /* Check bounds against machine memory size */
    if (phys_segment_end > m->memory_size) {
        return ERR_NO_PHYS_MEM;
    }

    /* Allocate and zero-initialize physical memory */
    for (uint32_t i = 0; i < rounded_size; i++) {
        nd500_bus_write8(m, phys_segment_base + i, 0);
    }

    /* Allocate page table (page-aligned at 2KB boundary) */
    /* Page table needs num_pages * 4 bytes (4 bytes per PTE) */
    uint32_t page_table_size = num_pages * 4;
    uint32_t page_table_aligned_size = (page_table_size + 2047) & ~2047u;
    
    /* Find free space for page table after segment */
    uint32_t page_table_base = (phys_segment_end + 2047) & ~2047u;
    uint32_t page_table_end = page_table_base + page_table_aligned_size;
    
    /* Check bounds */
    if (page_table_end > m->memory_size) {
        return ERR_NO_PHYS_MEM;
    }

    /* Zero-initialize page table */
    for (uint32_t i = 0; i < page_table_aligned_size; i++) {
        nd500_bus_write8(m, page_table_base + i, 0);
    }

    /* Create PTEs mapping virtual pages to physical pages */
    /* PTE format: [31:2]=PFN, [1]=unused, [0]=protection (0=RW, 1=RO) */
    /* Valid = (PFN != 0) - there is no separate valid bit */
    uint32_t segment_pfn = phys_segment_base >> PGSHIFT;
    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t pte_addr = page_table_base + (i * 4);
        uint32_t pfn = segment_pfn + i;
        uint32_t pte = (pfn << 2) | 0;  /* PFN | RW (protection=0) */
        nd500_bus_write32(m, pte_addr, pte);
    }

    /* Find free PSN (start from 102, after DOM loader uses 100-101) */
    int psn = find_free_psn(cpu, 102);
    if (psn < 0) {
        return ERR_NO_PST_INDEX;
    }

    /* Set up PST entry with PS_ASI mode, pointing to page table */
    uint32_t page_table_pfn = page_table_base >> PGSHIFT;
    nd500_mmu_set_pst_entry(cpu, psn, PS_ASI, page_table_pfn);

    /* Configure PCB data capability (writable) */
    uint16_t data_cap = psn | DC_WRP;
    nd500_mmu_set_data_capability(cpu, domain, assigned_segment, data_cap);

    /* Return assigned segment number */
    *out_assigned_segment = assigned_segment;

    return ERR_SUCCESS;
}

