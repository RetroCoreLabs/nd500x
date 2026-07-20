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

/* ─────────────────────────────────────────────────────────────────────────
 * Demand-grown segments (MON 412B FSCNT / 422B GSWSP)
 *
 * The ND-500 Reference Manual (ND-05.009.4, line 1237) describes a physical
 * segment as "any size from 2**11 to 2**27 bytes in units of 2k bytes (1 page)"
 * whose "pages can be moved (swapped) between main memory and secondary storage
 * as the need arises" - i.e. real hardware demand-pages a segment rather than
 * committing its whole extent at connect time.
 *
 * We previously sized a segment to the backing file's CURRENT length and mapped
 * every page up front in PS_ASI mode. That is wrong twice over for MON 412B
 * AccessType 1 ("uninitialized empty file", per 412B_FileAsSegment.yaml): the
 * file is empty BY DEFINITION, so its length says nothing about how much the
 * caller will address, and PS_ASI caps a segment at 512 pages (1 MB) because L1
 * must be 0. The ND linker connects its output domain that way and then writes
 * ~4.2 MB into it, faulting at the first address past the mapped pages:
 *     [MMU] TRAP: PS_ASI page fault! L1=4 must be 0! vaddr=0x18402004
 *
 * So MON-allocated segments are now built in PS_ADI (two-level) mode, which
 * spans the architectural 128 MB (L1 = 7 bits = 128 tables, L2 = 9 bits = 512
 * pages each), with only the pages that are actually needed mapped at creation.
 * A data fault inside such a segment allocates the missing page on the spot and
 * lets the access retry. Pages never touched cost nothing.
 * ───────────────────────────────────────────────────────────────────────── */

#define GROWABLE_MAX_SEGMENTS 64

typedef struct {
    int      in_use;
    uint8_t  domain;
    uint32_t segment;        /* logical segment number within the domain */
    uint32_t l1_table_base;  /* physical byte address of the L1 table */
    int      writable;       /* PTE protection to install on grown pages */
} GrowableSegment;

static GrowableSegment g_growable[GROWABLE_MAX_SEGMENTS];

/* Monotonic physical page watermark.
 *
 * find_highest_used_pfn() only walks PS_ASI page tables, so it cannot see the
 * data pages of a PS_ADI segment and would happily hand the same physical page
 * out twice. Every allocation below therefore comes from this watermark, which
 * only ever moves up. It is (re)seeded from find_highest_used_pfn() the first
 * time it is used for a given machine, so a fresh machine starts clean. */
static uint32_t g_next_free_pfn;
static void*    g_watermark_machine;

static void watermark_init(Nd500Cpu* cpu, Nd500Machine* m) {
    if (g_watermark_machine == (void*)m && g_next_free_pfn != 0) {
        return;  /* already seeded for this machine */
    }
    uint32_t highest = find_highest_used_pfn(cpu);
    uint32_t start = highest + 1;
    if (start < 1000) {
        start = 1000;  /* stay clear of the DOM loader's allocations */
    }
    g_next_free_pfn = start;
    g_watermark_machine = (void*)m;
    memset(g_growable, 0, sizeof(g_growable));
}

/* Take the next free physical page, zeroed. Returns 0 if memory is exhausted
 * (PFN 0 is never a valid allocation - the PTE format uses PFN==0 as invalid). */
static uint32_t watermark_alloc_page(Nd500Machine* m) {
    uint32_t pfn = g_next_free_pfn;
    uint32_t base = pfn << PGSHIFT;
    if (base + NBPG > m->memory_size) {
        return 0;
    }
    g_next_free_pfn++;
    for (uint32_t i = 0; i < NBPG; i++) {
        nd500_bus_write8(m, base + i, 0);
    }
    return pfn;
}

/* Install a PTE. PTE format: [31:2]=PFN, [0]=protection (0=RW, 1=RO);
 * a PFN of 0 means "not present". */
static void write_pte(Nd500Machine* m, uint32_t table_base, uint32_t index,
                      uint32_t pfn, uint32_t prot) {
    nd500_bus_write32(m, table_base + (index * 4), (pfn << 2) | prot);
}

static uint32_t read_pte_pfn(Nd500Machine* m, uint32_t table_base, uint32_t index) {
    uint32_t pte_addr = table_base + (index * 4);
    uint32_t v = ((uint32_t)nd500_bus_read8(m, pte_addr) << 24)
               | ((uint32_t)nd500_bus_read8(m, pte_addr + 1) << 16)
               | ((uint32_t)nd500_bus_read8(m, pte_addr + 2) << 8)
               |  (uint32_t)nd500_bus_read8(m, pte_addr + 3);
    return (v >> 2) & 0x3FFFFFFFu;
}

/* Map one page of a growable segment, creating its L2 table if needed.
 * Returns the physical PFN mapped, or 0 on failure. */
static uint32_t growable_map_page(Nd500Machine* m, GrowableSegment* g,
                                  uint32_t l1_index, uint32_t l2_index) {
    uint32_t prot = g->writable ? 0u : 1u;

    uint32_t l2_pfn = read_pte_pfn(m, g->l1_table_base, l1_index);
    if (l2_pfn == 0) {
        l2_pfn = watermark_alloc_page(m);   /* 512 entries * 4B = one page exactly */
        if (l2_pfn == 0) return 0;
        /* The L1 entry must stay writable regardless of the segment's data
         * protection: PS_ADI checks BOTH levels' protection bits on a write,
         * so a read-only L1 entry would deny writes to every page beneath it. */
        write_pte(m, g->l1_table_base, l1_index, l2_pfn, 0);
    }

    uint32_t l2_table_base = l2_pfn << PGSHIFT;
    uint32_t data_pfn = read_pte_pfn(m, l2_table_base, l2_index);
    if (data_pfn == 0) {
        data_pfn = watermark_alloc_page(m);
        if (data_pfn == 0) return 0;
        write_pte(m, l2_table_base, l2_index, data_pfn, prot);
    }
    return data_pfn;
}

static GrowableSegment* growable_find(uint8_t domain, uint32_t segment) {
    for (int i = 0; i < GROWABLE_MAX_SEGMENTS; i++) {
        if (g_growable[i].in_use && g_growable[i].domain == domain
            && g_growable[i].segment == segment) {
            return &g_growable[i];
        }
    }
    return NULL;
}

/**
 * Called from the MMU when a data access faults, BEFORE the trap is raised.
 * If the address falls inside a demand-grown segment, the missing page (and its
 * L2 table) is allocated and mapped so the access can be retried.
 *
 * Returns 1 if the fault was resolved, 0 to let the caller trap as usual.
 */
int nd500_segment_grow_on_fault(void* cpu_ptr, uint32_t virtual_addr, uint8_t domain) {
    Nd500Cpu* cpu = (Nd500Cpu*)cpu_ptr;
    if (!cpu || !cpu->machine) return 0;
    Nd500Machine* m = cpu->machine;

    uint32_t segment  = (virtual_addr >> SGSHIFT) & 0x1F;
    uint32_t l1_index = (virtual_addr >> L1_INDEX_SHIFT) & L1_INDEX_MASK;
    uint32_t l2_index = (virtual_addr >> L2_INDEX_SHIFT) & L2_INDEX_MASK;

    GrowableSegment* g = growable_find(domain, segment);
    if (!g) return 0;

    return growable_map_page(m, g, l1_index, l2_index) != 0;
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

    /* Number of pages to map up front. Growth beyond this happens on demand in
     * nd500_segment_grow_on_fault(), so this is a starting point, not a cap. */
    uint32_t rounded_size = (segment_size_bytes + (NBPG - 1)) & ~(uint32_t)(NBPG - 1);
    uint32_t num_pages = rounded_size / NBPG;
    if (num_pages == 0) num_pages = 1;

    watermark_init(cpu, m);

    /* Claim a growable-segment slot before committing any memory. */
    GrowableSegment* g = NULL;
    for (int i = 0; i < GROWABLE_MAX_SEGMENTS; i++) {
        if (!g_growable[i].in_use) { g = &g_growable[i]; break; }
    }
    if (!g) {
        return ERR_ILLEGAL_SEGMENT;  /* too many connected segments */
    }

    /* Find free PSN (start from 102, after DOM loader uses 100-101) */
    int psn = find_free_psn(cpu, 102);
    if (psn < 0) {
        return ERR_NO_PST_INDEX;
    }

    /* PS_ADI (two-level) so the segment can reach the architectural 128 MB:
     * L1 = 7 bits (128 tables) x L2 = 9 bits (512 pages) x 2 KB. The L1 table
     * is 128 entries * 4 bytes = 512 bytes, so one page holds it. */
    uint32_t l1_pfn = watermark_alloc_page(m);
    if (l1_pfn == 0) {
        return ERR_NO_PHYS_MEM;
    }

    g->in_use = 1;
    g->domain = domain;
    g->segment = assigned_segment;
    g->l1_table_base = l1_pfn << PGSHIFT;
    g->writable = writable;

    /* Map the initial pages. */
    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t l1_index = (i >> 9) & L1_INDEX_MASK;
        uint32_t l2_index = i & L2_INDEX_MASK;
        if (growable_map_page(m, g, l1_index, l2_index) == 0) {
            g->in_use = 0;
            return ERR_NO_PHYS_MEM;
        }
    }

    nd500_mmu_set_pst_entry(cpu, psn, PS_ADI, l1_pfn);

    /* Configure PCB data capability. DC_WRP set = write permitted. */
    uint16_t data_cap = (uint16_t)(psn | (writable ? DC_WRP : 0));
    nd500_mmu_set_data_capability(cpu, domain, assigned_segment, data_cap);

    *out_assigned_segment = assigned_segment;
    /* Pages are no longer physically contiguous, so there is no single base a
     * caller could memcpy into. Callers that need to preload content use
     * nd500_segment_write_bytes(), which walks the mapping page by page. */
    if (out_phys_base) *out_phys_base = 0;

    /* Opt-in layout dump (ND500X_SEG_DUMP) to hunt physical overlap between GSWSP segments. */
    {
        static int dbg = -1;
        if (dbg < 0) { const char* e = getenv("ND500X_SEG_DUMP"); dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
        if (dbg) {
            uint32_t vbase = (uint32_t)assigned_segment << 27; /* VA seg field */
            fprintf(stderr, "[SEG] ic=%llu CED=%u dom=%u seg=%u vbase=%08X reqBytes=%u rounded=%u initPages=%u "
                    "mode=ADI l1tbl=%08X psn=%d nextFreePfn=%u\n",
                    (unsigned long long)cpu->instruction_count, (unsigned)cpu->CED, domain, assigned_segment, vbase,
                    segment_size_bytes, rounded_size, num_pages,
                    g->l1_table_base, psn, g_next_free_pfn); (void)0;
            fflush(stderr);
        }
    }

    return ERR_SUCCESS;
}

/**
 * Write bytes into a demand-grown segment at a segment-relative offset,
 * mapping pages as it goes. Used by FSCNT to preload a file's contents now that
 * a segment's pages are not physically contiguous.
 *
 * Returns 0 on success, or a SINTRAN error code.
 */
static int nd500_segment_write_bytes(Nd500Machine* m, GrowableSegment* g,
                                     uint32_t offset, const uint8_t* data, uint32_t len)
{
    uint32_t written = 0;
    while (written < len) {
        uint32_t seg_off  = offset + written;
        uint32_t page_idx = seg_off / NBPG;
        uint32_t in_page  = seg_off % NBPG;
        uint32_t chunk    = NBPG - in_page;
        if (chunk > len - written) chunk = len - written;

        uint32_t l1_index = (page_idx >> 9) & L1_INDEX_MASK;
        uint32_t l2_index = page_idx & L2_INDEX_MASK;
        uint32_t pfn = growable_map_page(m, g, l1_index, l2_index);
        if (pfn == 0) return ERR_NO_PHYS_MEM;

        uint32_t phys = (pfn << PGSHIFT) + in_page;
        for (uint32_t i = 0; i < chunk; i++) {
            nd500_bus_write8(m, phys + i, data[written + i]);
        }
        written += chunk;
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

    /* Load types (0/2/3): copy the file's bytes into the segment verbatim.
     * The segment's pages are not physically contiguous (demand-grown, PS_ADI),
     * so this goes through the mapping rather than a flat physical base. Every
     * page allocated by the core allocator is already zeroed. */
    GrowableSegment* g = growable_find(domain == 0xFF ? (uint8_t)((Nd500Cpu*)cpu_ptr)->CED : domain,
                                       *out_assigned_segment);
    if (!g) { fclose(fp); return ERR_ILLEGAL_SEGMENT; }

    rewind(fp);
    uint8_t buf[NBPG];
    uint32_t off = 0;
    while (off < load_bytes) {
        uint32_t want = load_bytes - off;
        if (want > sizeof(buf)) want = sizeof(buf);
        size_t got = fread(buf, 1, want, fp);
        if (got == 0) break;
        int wrc = nd500_segment_write_bytes(m, g, off, buf, (uint32_t)got);
        if (wrc != ERR_SUCCESS) { fclose(fp); return wrc; }
        off += (uint32_t)got;
    }
    fclose(fp);
    return ERR_SUCCESS;
}

