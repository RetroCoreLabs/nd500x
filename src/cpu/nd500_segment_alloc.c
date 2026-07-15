#include <stdlib.h>
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

            /* Scan page table entries - stop at first invalid (zero) entry.
             * Valid PTEs are contiguous from start; unused slots are zero.
             * In ND-500 PTE format, PFN=0 indicates invalid/unmapped page. */
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

                /* Stop at first invalid entry - prevents reading garbage */
                if (pte_pfn == 0) {
                    break;
                }

                if (pte_pfn > highest_pfn) {
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
/* Shared core: allocate an MMU/PST-backed data segment and wire the caller
 * domain's data capability. `writable` selects RW (DC_WRP + PTE prot 0) vs RO
 * (no DC_WRP + PTE prot 1). Returns the physical base of the segment via
 * out_phys_base so a caller (FSCNT) can pre-load file bytes. Used by both
 * nd500_mon_allocate_segment (empty scratch, 422B GSWSP) and
 * nd500_mon_connect_file_as_segment (file-backed, 412B FSCNT). */
static int alloc_backed_segment(void* cpu_ptr, void* machine_ptr, uint8_t domain,
    uint32_t requested_segment, uint32_t segment_size_bytes, int writable,
    uint32_t* out_assigned_segment, uint32_t* out_phys_base)
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
    uint32_t pte_prot = writable ? 0u : 1u;  /* 0=RW, 1=RO */
    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t pte_addr = page_table_base + (i * 4);
        uint32_t pfn = segment_pfn + i;
        uint32_t pte = (pfn << 2) | pte_prot;
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

    /* Configure PCB data capability. DC_WRP set = write permitted. */
    uint16_t data_cap = (uint16_t)(psn | (writable ? DC_WRP : 0));
    nd500_mmu_set_data_capability(cpu, domain, assigned_segment, data_cap);

    /* Return assigned segment number + physical base (for file preload) */
    *out_assigned_segment = assigned_segment;
    if (out_phys_base) *out_phys_base = phys_segment_base;

    /* Opt-in layout dump (ND500X_SEG_DUMP) to hunt physical overlap between GSWSP segments. */
    {
        static int dbg = -1;
        if (dbg < 0) { const char* e = getenv("ND500X_SEG_DUMP"); dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
        if (dbg) {
            uint32_t vbase = (uint32_t)assigned_segment << 27; /* VA seg field */
            fprintf(stderr, "[SEG] ic=%llu CED=%u dom=%u seg=%u vbase=%08X reqBytes=%u rounded=%u pages=%u "
                    "phys=[%08X,%08X) ptbl=[%08X,%08X) psn=%d highestPfn=%u\n",
                    (unsigned long long)cpu->instruction_count, (unsigned)cpu->CED, domain, assigned_segment, vbase,
                    segment_size_bytes, rounded_size, num_pages,
                    phys_segment_base, phys_segment_end, page_table_base, page_table_end,
                    psn, highest_pfn); (void)0;
            fflush(stderr);
        }
    }

    return ERR_SUCCESS;
}

/**
 * MON callback: Allocate an EMPTY scratch segment (422B GSWSP). Thin wrapper
 * over the shared core with a writable capability.
 */
int nd500_mon_allocate_segment(void* cpu_ptr, void* machine_ptr, uint8_t domain,
    uint32_t requested_segment, uint32_t segment_size_bytes,
    uint32_t* out_assigned_segment)
{
    uint32_t phys_base = 0;
    return alloc_backed_segment(cpu_ptr, machine_ptr, domain, requested_segment,
        segment_size_bytes, /*writable=*/1, out_assigned_segment, &phys_base);
}

/**
 * MON callback: Connect an open FILE as a data segment (412B FSCNT). Allocates
 * an MMU/PST-backed segment (same wiring as GSWSP) then pre-loads the file's
 * bytes into its physical pages.
 *
 *   access_type: 0 = initial data (load file), 1 = uninitialized (zero),
 *                2 = primarily sequential (load), 3 = combination 1+2 (load).
 *   writable:    RO vs RW capability comes from the FILE's open mode, passed by
 *                the handler (NOT from access_type).
 * The segment is zeroed by the core allocator; for load types we overwrite the
 * head with the file bytes (raw byte stream - copied verbatim, no byte-swap).
 * Returns 0 on success, SINTRAN error code otherwise.
 */
int nd500_mon_connect_file_as_segment(void* cpu_ptr, void* machine_ptr, uint8_t domain,
    uint32_t requested_segment, uint32_t access_type, int writable,
    const char* host_path, uint32_t file_size_bytes,
    uint32_t* out_assigned_segment)
{
    if (!cpu_ptr || !machine_ptr || !out_assigned_segment || !host_path) {
        return ERR_ILLEGAL_ADDRESS;
    }
    Nd500Machine* m = (Nd500Machine*)machine_ptr;

    /* If the size is unknown (scratch may report 0), measure the host file. */
    uint32_t load_bytes = file_size_bytes;
    long fsz = -1;
    FILE* fp = fopen(host_path, "rb");
    if (fp) {
        if (fseek(fp, 0, SEEK_END) == 0) { fsz = ftell(fp); }
        if (fsz >= 0 && (load_bytes == 0 || (uint32_t)fsz < load_bytes)) {
            load_bytes = (uint32_t)fsz;
        }
    }
    /* Segment must be at least one page even for an empty file. */
    uint32_t seg_bytes = (load_bytes == 0) ? 1u : load_bytes;

    uint32_t phys_base = 0;
    int rc = alloc_backed_segment(cpu_ptr, machine_ptr, domain, requested_segment,
        seg_bytes, writable, out_assigned_segment, &phys_base);
    if (rc != ERR_SUCCESS) { if (fp) fclose(fp); return rc; }

    /* access_type 1 = uninitialized: leave the (already zeroed) pages. */
    if (access_type == 1 || !fp || load_bytes == 0) {
        if (fp) fclose(fp);
        return ERR_SUCCESS;
    }

    /* Load types (0/2/3): copy the file's bytes into the segment's physical
     * pages verbatim. The core allocator already zeroed the tail. */
    rewind(fp);
    for (uint32_t off = 0; off < load_bytes; off++) {
        int c = fgetc(fp);
        if (c == EOF) break;
        nd500_bus_write8(m, phys_base + off, (uint8_t)c);
    }
    fclose(fp);
    return ERR_SUCCESS;
}

