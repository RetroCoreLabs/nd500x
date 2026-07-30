/*
 * ND-500 DOM/SEG File Loading - Unified Implementation
 *
 * This module provides a single function for loading DOM/SEG files into
 * the emulator. After loading segments into physical memory, it performs
 * the complete MMU and domain system initialization required for execution:
 *
 *   1. Copy DATA/PROG segments to physical memory
 *   2. Initialize domain system (CED=0, CAD=0 for kernel domain)
 *   3. Create page tables for virtual-to-physical translation
 *   4. Configure segment capabilities in the PCB (Process Control Block)
 *   5. Set up segment 31 with PC_IND flag for SINTRAN MON call interception
 *   6. Enable program and data MMU
 *
 * IMPORTANT: Segment 31 setup happens DURING this DOM loading process,
 * not before. The PC_IND capability flag must be configured before any
 * code attempts to execute MON calls, otherwise CALL to 0xF8xxxxxx will
 * cause an MMU fault instead of being intercepted.
 *
 * MMU SETUP: For ProgramAndData segments (segments with both PROG and DATA),
 * we create SEPARATE PST entries for program and data access. This allows
 * instruction fetches to read from PROG pages while data accesses read from
 * DATA pages - even though both use the same virtual segment number.
 *
 * Used by both:
 *   - debugger commands.c (loaddom command)
 *   - frontend nd500x.c (--dom flag)
 */

#include "ndlib.h"
#include "nd500_dom.h"
#include "../cpu/nd500_mmu.h"
#include "../cpu/nd500_phys_alloc.h"
#include "../cpu/nd500_domain.h"
#include "../machine/machine_protos.h"
#include "../debugger/debugger.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* Segment type enum - matches C# SegmentType */
#define SEG_TYPE_UNUSED         0
#define SEG_TYPE_PROG_ONLY      1
#define SEG_TYPE_DATA_ONLY      2
#define SEG_TYPE_PROG_AND_DATA  3

/* SINTRAN Window configuration */
#define SINTRAN_WINDOW_PAGES  8   /* 8 pages = 16KB */

/* Extra zeroed pages reserved ABOVE a segment's initialized DATA for the
 * program's stack/heap/bss growth at runtime. The DOM segment descriptor
 * carries only the initialized (file) data size (SEG_OFF_SZ); the uninitialized
 * growth region that the program's stack (frame register B) and heap expand into
 * is NOT described (FLA/FUA/MINP/MAXP are 0 in the vendor DOMs). Without this
 * reserve a program with deep recursion or a large heap - e.g. the NC C compiler
 * during code generation - grows its stack past the last mapped data page and
 * takes a page fault (VA just above data_size), then its own page-fault handler
 * faults again on unmapped work-segments -> fatal. 512 pages = 1MB is generous
 * for the ND-500 toolchain and fits comfortably in the 16MB machine. */
#define DATA_GROWTH_RESERVE_PAGES  512
#define SINTRAN_WINDOW_SIZE   (SINTRAN_WINDOW_PAGES * 2048)

/*
 * Setup SINTRAN Window (Segment 31) for system data access.
 *
 * This function configures DC[31] to allow programs to access SINTRAN system
 * data structures at virtual addresses 0xF8xxxxxx (segment 31).
 *
 * ARCHITECTURAL NOTE:
 * In the real ND-500/ND-100 dual processor system:
 *   - ND-100 runs SINTRAN III and manages RT scheduling
 *   - RT descriptions and system tables live in ND-100 memory (DPIT)
 *   - ND-500 accesses this data through shared memory or H RIOM instruction
 *   - When ND-100 suspends an ND-500 process, it saves registers to RT description
 *
 * In the standalone ND-500 emulator (no ND-100 integration):
 *   - We allocate physical memory to simulate SINTRAN tables
 *   - MON calls like GETRT (30B) return addresses in this region (0xF8001000)
 *   - Programs can read/write system structures at these addresses
 *
 * For future ND-100 integration:
 *   - This function can be modified to map to actual ND-100 memory
 *   - The sintran_phys_base would point to shared memory region
 *   - Page table PTEs would reference ND-100 accessible memory
 *
 * Physical layout within SINTRAN window:
 *   Offset 0x0000-0x0FFF: Reserved
 *   Offset 0x1000-0x10FF: RT description (MON 30B returns 0xF8001000)
 *   Offset 0x1100-0x3FFF: Available for other system structures
 *
 * Parameters:
 *   m              - Machine for memory allocation
 *   cpu            - CPU to configure MMU
 *   domain         - Domain to set DC[31] for
 *   next_psn       - Next available PST number (updated on return)
 *   log_callback   - Optional logging callback
 *   log_context    - Context for logging
 *
 * Returns: 0 on success, -1 on error
 */
/* Claim the next FREE PST entry at or after *from (which is advanced past it).
 * A PST entry is free when it describes nothing: index_mode 0 and PFN 0.
 * Returns -1 when the table is full. */
static int alloc_free_psn(Nd500Cpu* cpu, int* from) {
    for (int psn = *from; psn < MAX_PST; psn++) {
        PhysicalSegmentTableEntry e = nd500_mmu_get_pst_entry(cpu, psn);
        if (e.index_mode == 0 && e.physical_pfn == 0) {
            *from = psn + 1;
            return psn;
        }
    }
    return -1;
}

static int setup_sintran_window(
    Nd500Machine* m,
    Nd500Cpu* cpu,
    int domain,
    int* next_psn,
    void (*log_callback)(void* ctx, const char* fmt, ...),
    void* log_context)
{
    if (!m || !cpu || !next_psn) {
        return -1;
    }

    /* Window pages and the page table describing them both come from the
     * machine's page allocator, zeroed: a recycled page still holds the
     * previous owner's bytes, and a page table with leftovers after its last
     * real entry maps garbage instead of faulting. */
    uint32_t win_pages = (SINTRAN_WINDOW_SIZE + 2047) / 2048;
    uint32_t win_pfn = nd500_phys_alloc_pages(m, win_pages, /*zero=*/1);
    if (win_pfn == 0) return -1;
    uint32_t sintran_phys_base = win_pfn << 11;

    uint32_t pt_pages = (SINTRAN_WINDOW_PAGES * 4 + 2047) / 2048;
    uint32_t pt_pfn = nd500_phys_alloc_pages(m, pt_pages, /*zero=*/1);
    if (pt_pfn == 0) {
        nd500_phys_free_pages(m, win_pfn, win_pages);
        return -1;
    }
    uint32_t sintran_pt_base = pt_pfn << 11;

    /* Fill page table with PTEs for SINTRAN window pages.
     * ND-500 hardware pte.h format (per commit 6b3d4fb): pg_prot@31, pg_pfnum@[29:0].
     * (The MMU + write_pte were realigned to this; these inline sites must match.)
     */
    for (uint32_t p = 0; p < SINTRAN_WINDOW_PAGES; p++) {
        uint32_t pte_addr = sintran_pt_base + p * 4;
        uint32_t pfn = (sintran_phys_base >> 11) + p;
        uint32_t pte = (pfn & 0x3FFFFFFFu);  /* prot=0 (RW), pfn@[29:0] */
        nd500_bus_write32(m, pte_addr, pte);
    }

    /* Allocate PST entry for SINTRAN window */
    int sintran_psn = alloc_free_psn(cpu, next_psn);
    if (sintran_psn < 0) {
        nd500_phys_free_pages(m, pt_pfn, pt_pages);
        nd500_phys_free_pages(m, win_pfn, win_pages);
        return -1;
    }
    nd500_mmu_set_pst_entry(cpu, sintran_psn, PS_ASI, sintran_pt_base >> 11);

    /* Set DC[31] to allow data access to SINTRAN window */
    nd500_mmu_set_data_capability(cpu, domain, 31, sintran_psn | DC_WRP);

    if (log_callback) {
        log_callback(log_context, "  Seg 31 SINTRAN Window: %u pages @ phys 0x%08X, PSN %d",
                     SINTRAN_WINDOW_PAGES, sintran_phys_base, sintran_psn);
    }

    return 0;
}

/* Per-segment tracking during DOM load */
typedef struct {
    int has_prog;
    int has_data;
    uint32_t prog_phys_base;   /* Physical address where PROG was loaded */
    uint32_t prog_size;
    uint32_t data_phys_base;   /* Physical address where DATA was loaded */
    uint32_t data_size;
    int seg_type;              /* SEG_TYPE_* */
    int psn_prog;              /* PST number for PROG (if allocated) */
    int psn_data;              /* PST number for DATA (if allocated) */
} SegmentInfo;

/*
 * Load DOM/SEG file into machine and configure MMU for execution.
 *
 * This function:
 *   1. Assumes ndlib_load_dom_header() and ndlib_load_dom_segments() already called
 *   2. Copies DATA segments to physical memory at 0x00000000
 *   3. Copies PROG segments to physical memory (page-aligned after DATA)
 *   4. Initializes domain system (CED, CAD)
 *   5. Sets up MMU page tables using PS_ASI (single-level paging)
 *   6. Configures segment 31 for SINTRAN MON call interception
 *   7. Enables MMU
 *   8. Sets PC to start address from header
 *
 * Parameters:
 *   m              - Machine to load into
 *   cpu            - CPU to configure (domain and MMU)
 *   target_domain  - Domain to load into: -1 = auto-allocate (1-255), 0-255 = specific domain
 *   log_callback   - Optional callback for progress messages (NULL to suppress)
 *   log_context    - Context passed to log_callback
 *   out_start_addr - Returns start address from header (may be NULL)
 *   out_domain     - Returns actual domain loaded into (may be NULL)
 *
 * Returns: 0 on success, -1 on error
 */
int ndlib_dom_load_to_machine(
    Nd500Machine* m,
    Nd500Cpu* cpu,
    int target_domain,
    void (*log_callback)(void* ctx, const char* fmt, ...),
    void* log_context,
    uint32_t* out_start_addr,
    int* out_domain)
{
    if (!m || !cpu) {
        return -1;
    }

    /* Get header - must have been loaded already */
    const nd500_header_t* hdr = ndlib_get_dom_header();
    if (!hdr || !ndlib_dom_is_loaded()) {
        return -1;
    }

    /* ========================================================================
     * Domain Allocation (like C# AllocateDomain)
     *
     * Domain 0 is reserved for kernel. User programs should load into 1-255.
     * If target_domain is -1, auto-allocate first free domain.
     * ======================================================================== */
    int domain;
    if (target_domain < 0) {
        /* Auto-allocate domain */
        domain = nd500_domain_allocate(cpu);
        if (domain < 0) {
            if (log_callback) {
                log_callback(log_context, "Error: No free domains available");
            }
            return -1;
        }
    } else if (target_domain >= 256) {
        if (log_callback) {
            log_callback(log_context, "Error: Invalid domain number %d (must be 0-255)", target_domain);
        }
        return -1;
    } else {
        domain = target_domain;
        /* Mark domain as in use */
        cpu->domains_in_use[domain] = 1;
    }

    if (out_domain) {
        *out_domain = domain;
    }

    int is_dom = ndlib_dom_is_dom_file();
    uint32_t start_addr = nd500_read32(&hdr->raw[0xD8]);

    /* Read trap registers from DOM header */
    uint32_t tha    = nd500_read32(&hdr->raw[0xE0]);
    uint32_t mte2   = nd500_read32(&hdr->raw[0xE4]);
    uint32_t mte1   = nd500_read32(&hdr->raw[0xE8]);
    uint32_t ote2   = nd500_read32(&hdr->raw[0xEC]);
    uint32_t ote1   = nd500_read32(&hdr->raw[0xF0]);
    uint32_t cte2   = nd500_read32(&hdr->raw[0xF4]);
    uint32_t cte1   = nd500_read32(&hdr->raw[0xF8]);
    uint32_t temm2  = nd500_read32(&hdr->raw[0xFC]);
    uint32_t temm1  = nd500_read32(&hdr->raw[0x100]);

    if (out_start_addr) {
        *out_start_addr = start_addr;
    }

    /* ========================================================================
     * Physical Memory Layout (matching C# RetroCore):
     *   For each segment: [PROGRAM bytes][DATA bytes] contiguous
     *   PROGRAM is loaded first, DATA immediately after
     *
     * This matches CpuND500.Loader.cs lines 2134-2150:
     *   physAddr = PFNToPhysical(basePFN)
     *   CopyToPhysicalMemory(physAddr, ProgramData)         // PROGRAM first
     *   CopyToPhysicalMemory(physAddr + programSize, DataData)  // DATA after
     * ======================================================================== */

    int max_segs = is_dom ? 32 : 1;
    int found = 0;

    /* Per-segment tracking array */
    SegmentInfo seg_info[32];
    memset(seg_info, 0, sizeof(seg_info));

    /* Load each segment: PROGRAM first, then DATA immediately after
     *
     * Matching C# RetroCore behavior (CpuND500.Loader.cs lines 2134-2150):
     * - PROGRAM is loaded at page-aligned base
     * - DATA is loaded immediately after PROGRAM (at base + programSize bytes)
     *
     * The page table for DATA points to (basePFN + programPages), which is
     * page-aligned. This means there's an offset between where DATA is copied
     * and where the page table thinks it is. This matches C# behavior.
     */
    for (int i = 0; i < max_segs; i++) {
        uint32_t prog_size = 0, prog_addr = 0;
        uint32_t data_size = 0, data_addr = 0;
        const uint8_t* prog_data = ndlib_dom_get_segment_data(i, &prog_size, &prog_addr);
        const uint8_t* dat_data = ndlib_dom_get_data_section(i, &data_size, &data_addr);

        if ((!prog_data || prog_size == 0) && (!dat_data || data_size == 0)) {
            continue;  /* Skip empty segments */
        }

        /* Take this segment's physical pages from the machine's allocator.
         *
         * Every DOM used to be placed at the SAME fixed base (phys 0x800) off a
         * bump cursor, so loading a second domain wrote over the first one's
         * resident image - which is why a nested UECOM run had to copy all of
         * physical memory out and back around itself, and why a sub-program
         * could only return results through files. With real allocation each
         * domain gets its own frames and several can be live at once.
         *
         * PROG and DATA stay in ONE contiguous block per segment: DATA is
         * placed at a page boundary after PROG and the DATA mapping adopts a
         * contiguous PFN run from there. */
        uint32_t prog_bytes = (prog_data && prog_size) ? prog_size : 0;
        uint32_t data_bytes = (dat_data && data_size) ? data_size : 0;
        uint32_t seg_pages = ((prog_bytes + 2047) / 2048)
                           + ((data_bytes + 2047) / 2048);
        if (seg_pages == 0) seg_pages = 1;

        uint32_t seg_pfn = nd500_phys_alloc_pages(m, seg_pages, /*zero=*/1);
        if (seg_pfn == 0) {
            if (log_callback) {
                log_callback(log_context,
                    "  Segment %d: out of physical memory (%u pages needed, %u free)",
                    i, seg_pages, nd500_phys_pages_free(m));
            }
            return -1;
        }
        uint32_t seg_phys_base = seg_pfn << 11;
        uint32_t prog_pages = 0;
        uint32_t byte_offset = 0;

        /* Copy PROGRAM first (if present) */
        if (prog_data && prog_size > 0) {
            seg_info[i].has_prog = 1;
            seg_info[i].prog_phys_base = seg_phys_base;  /* Page-aligned */
            seg_info[i].prog_size = prog_size;
            prog_pages = (prog_size + 2047) / 2048;

            for (uint32_t j = 0; j < prog_size; j++) {
                nd500_bus_write8(m, seg_info[i].prog_phys_base + j, prog_data[j]);
            }
            if (log_callback) {
                log_callback(log_context, "  Segment %d PROG: %u bytes (%u pages) -> phys 0x%08X",
                             i, prog_size, prog_pages, seg_info[i].prog_phys_base);
            }
            byte_offset = prog_size;  /* DATA follows immediately after PROG bytes */
            found++;
        }

        /* Copy DATA at page-aligned address after PROGRAM
         * The page tables use PFN which assumes page alignment.
         * If DATA isn't page-aligned, the page table PTEs will point to wrong offsets.
         */
        if (dat_data && data_size > 0) {
            seg_info[i].has_data = 1;
            /* Round byte_offset up to page boundary for DATA placement */
            uint32_t aligned_offset = (byte_offset + 2047) & ~2047u;
            seg_info[i].data_phys_base = seg_phys_base + aligned_offset;
            seg_info[i].data_size = data_size;
            /* Store prog_pages for page table setup */
            seg_info[i].prog_size = prog_size;  /* Need this for page table offset calc */

            for (uint32_t j = 0; j < data_size; j++) {
                nd500_bus_write8(m, seg_info[i].data_phys_base + j, dat_data[j]);
            }
            if (log_callback) {
                uint32_t data_pages = (data_size + 2047) / 2048;
                log_callback(log_context, "  Segment %d DATA: %u bytes (%u pages) -> phys 0x%08X",
                             i, data_size, data_pages, seg_info[i].data_phys_base);
            }
            byte_offset = aligned_offset + data_size;
            found++;
        }
        (void)byte_offset;   /* segments no longer share one bump cursor */
    }

    /* Determine segment types */
    for (int i = 0; i < max_segs; i++) {
        if (seg_info[i].has_prog && seg_info[i].has_data) {
            seg_info[i].seg_type = SEG_TYPE_PROG_AND_DATA;
        } else if (seg_info[i].has_prog) {
            seg_info[i].seg_type = SEG_TYPE_PROG_ONLY;
        } else if (seg_info[i].has_data) {
            seg_info[i].seg_type = SEG_TYPE_DATA_ONLY;
        }
        /* else SEG_TYPE_UNUSED (0) - default */
    }

    if (found == 0) {
        if (log_callback) {
            log_callback(log_context, "  (no segments found)");
        }
        return 0;
    }

    /* ========================================================================
     * Initialize domain system (required for MMU and MON calls)
     * ======================================================================== */
    nd500_domain_init(cpu);

    /* Set domain registers to target domain */
    cpu->CED = (uint32_t)domain;
    cpu->CAD = (uint32_t)domain;

    if (log_callback) {
        log_callback(log_context, "Loading into domain %d (CED=%d, CAD=%d)", domain, domain, domain);
    }

    /* Initialize trap registers from DOM header */
    cpu->THA   = tha;
    cpu->MTE1  = mte1;  cpu->MTE2  = mte2;
    cpu->OTE1  = ote1;  cpu->OTE2  = ote2;
    cpu->CTE1  = cte1;  cpu->CTE2  = cte2;
    cpu->TEMM1 = temm1; cpu->TEMM2 = temm2;

    /* A freshly placed program starts with a CLEAN status register. ST1/ST2
     * carry sticky trap-status bits (Table 10) that the previous program in
     * this session may have left set (e.g. an ignorable trap it never enabled
     * in OTE). Combined with the NEW program's OTE from the header, a stale
     * bit dispatched a spurious trap on the program's FIRST instruction
     * (check_pending_traps fires on ST & OTE): PLANC left a pending bit and
     * the next FILE-COMPARE stormed reading its THA vector before executing
     * anything. Same for a stale in-trap-handler flag. */
    cpu->ST1 = 0;
    cpu->ST2 = 0;
    cpu->in_trap_handler = false;
    /* Same reasoning for the dispatch->ENTT interlock: a stale value would make
     * the new program's first ENTT look valid, and would block cross-domain trap
     * dispatch until something cleared it. */
    cpu->trap_dispatch_pending = 0;

    /* ========================================================================
     * Set up MMU page tables using PS_ASI (single-level paging)
     *
     * For each segment with content, create a separate PST entry and page table.
     * ProgramAndData segments get TWO PST entries: one for PROG, one for DATA.
     * This matches the C# RetroCore implementation.
     * ======================================================================== */

    /* PST entries are claimed by SEARCHING for free ones rather than counting up
     * from a fixed index: with more than one domain resident (a DOM that starts
     * another DOM, or a new command), a fixed start would hand the second load
     * the entries the first one is still translating through. */
    int next_psn = 100;  /* first index considered, not a running counter */

    /* DATA segments are built PS_ASI (single-level) with a fixed page reserve,
     * in-line below. This is the proven-good aa5cd5e mapping, restored after the
     * PS_ADI "growable DATA" rewrite (commit 951237d) regressed the real NC C
     * compiler (codegen crash -> jumpg -> PC=0).
     *
     * ROOT CAUSE (reproduced GOOD-vs-BAD): the divergence is TRAP-vs-SILENT-MAP,
     * NOT a data-content difference. NC's codegen deliberately runs PAST the end
     * of its owned DATA region (e.g. seg-0 VA 0x02200000, ~34 MB) and RELIES on
     * the resulting PAGE FAULT being dispatched to NC's OWN PGF trap handler
     * (via its THA), which it recovers from - PS_ASI faults there (L1 != 0 is
     * unmapped) and NC exits cleanly. The growable rewrite instead SILENTLY grew
     * a zeroed page for that access (through the seg-0->seg-1 growable alias), so
     * NC's handler never ran, NC diverged, and it crashed. The earlier "zeroed vs
     * mapped content" theory was wrong: the seg-4 MON-scratch grow is byte-
     * identical GOOD and BAD. PTE encoding kept in the current hardware format
     * (pg_prot@31, pg_pfnum@[29:0]).
     *
     * LIMITATION: PS_ASI forces L1==0, so a single DATA segment here is capped at
     * 1 MB (512 pages). This is adequate in practice - NC's ~34 MB scratch and
     * the ND linker's ~4 MB output both live in MON-CONNECTED segments (2+/seg 3),
     * which are demand-growable and unaffected by this cap; no known program needs
     * >1 MB of seg-0/1 DSEG. The cap only bites a hypothetical large seg-0/1 DSEG.
     *
     * HOW TO EXTEND (>1 MB seg-0/1 without re-regressing NC): build seg-0/1 DATA
     * as PS_ADI two-level, EAGERLY mapping the adopted contiguous init data + a
     * bounded contiguous zeroed reserve, and do NOT register seg-0/1 in
     * g_growable - so an access past the owned extent still TRAPS (preserving NC's
     * fault boundary) while the reserve may span L1>0. No MMU-core change needed
     * (the PS_ADI path already traps when grow_on_fault declines). Full design +
     * change sites + acceptance test are in the NDInsight notes, under
     * SINTRAN/ND500, "PLAN-nd500x-growable-DATA-option2-redesign-2026-07-26.md". */

    /* Process each segment - create page tables and PST entries */
    for (int i = 0; i < max_segs; i++) {
        if (seg_info[i].seg_type == SEG_TYPE_UNUSED) {
            continue;
        }

        /* Create page table and PST entry for PROG (if segment has program) */
        if (seg_info[i].has_prog) {
            uint32_t prog_pages = (seg_info[i].prog_size + 2047) / 2048;
            if (prog_pages == 0) prog_pages = 1;

            /* Allocate the page table, ZEROED. A recycled page still holds the
             * previous owner's bytes, and PTE validity here is "PFN != 0", so
             * leftovers after the last real entry would MAP GARBAGE instead of
             * faulting on an access past the segment's pages. */
            uint32_t pt_pages = (prog_pages * 4 + 2047) / 2048;
            uint32_t pt_pfn = nd500_phys_alloc_pages(m, pt_pages, /*zero=*/1);
            if (pt_pfn == 0) {
                if (log_callback) {
                    log_callback(log_context,
                        "  Seg %d PROG: out of physical memory for page table", i);
                }
                return -1;
            }
            uint32_t pt_base = pt_pfn << 11;

            /* Fill page table - PTEs for PROG pages.
             * ND-500 hardware pte.h format (commit 6b3d4fb): pg_prot@31, pg_pfnum@[29:0]. */
            for (uint32_t p = 0; p < prog_pages; p++) {
                uint32_t pte_addr = pt_base + p * 4;
                uint32_t pfn = (seg_info[i].prog_phys_base >> 11) + p;
                uint32_t pte = (1u << 31) | (pfn & 0x3FFFFFFFu);  /* prot=1 (RO code) */
                nd500_bus_write32(m, pte_addr, pte);
            }

            /* Allocate PST entry and set up */
            seg_info[i].psn_prog = alloc_free_psn(cpu, &next_psn);
            if (seg_info[i].psn_prog < 0) return -1;
            nd500_mmu_set_pst_entry(cpu, seg_info[i].psn_prog, PS_ASI, pt_base >> 11);

            /* Set program capability for this segment */
            nd500_mmu_set_program_capability(cpu, domain, i, seg_info[i].psn_prog | PC_DIR);

            if (log_callback) {
                log_callback(log_context, "  Seg %d PROG: %u pages, PT @ 0x%08X, PSN %d",
                             i, prog_pages, pt_base, seg_info[i].psn_prog);
            }
        }

        /* Create SEPARATE PST entry + data capability for DATA (if segment has
         * data). The page tables themselves are built in PASS 2 below, as
         * BOUNDED PS_ADI two-level.
         * PS_ADI lifts the PS_ASI 1 MB cap that truncated real DOMs
         * (FILE-COMPARE ships 1003 pages / 2 MB of initialized seg-1 DATA,
         * its THA vector in the unreachable upper half), while staying
         * NON-growable so an access past data+reserve still traps to the
         * guest THA exactly like PS_ASI did (the fault boundary NC's codegen
         * relies on - see the block comment above). */
        if (seg_info[i].has_data) {
            seg_info[i].psn_data = alloc_free_psn(cpu, &next_psn);
            if (seg_info[i].psn_data < 0) return -1;
            nd500_mmu_set_data_capability(cpu, domain, i, seg_info[i].psn_data | DC_WRP);
        }

        /* ProgramOnly segments: set DC to same as PC for read-only data access */
        if (seg_info[i].seg_type == SEG_TYPE_PROG_ONLY) {
            nd500_mmu_set_data_capability(cpu, domain, i, seg_info[i].psn_prog);
            if (log_callback) {
                log_callback(log_context, "  Seg %d DC -> PSN %d (read-only, same as PC)",
                             i, seg_info[i].psn_prog);
            }
        }
    }

    /* FORTRAN-500 compatibility: if segment 0 is empty and segment 1 has data,
     * alias DC[0] to segment 1's data for programs that access data via 0x00xxxxxx */
    if (seg_info[0].seg_type == SEG_TYPE_UNUSED && seg_info[1].has_data) {
        nd500_mmu_set_data_capability(cpu, domain, 0, seg_info[1].psn_data | DC_WRP);
        if (log_callback) {
            log_callback(log_context, "  DC[0] aliased to Seg 1 DATA (PSN %d) - FORTRAN compatibility",
                         seg_info[1].psn_data);
        }
    }

    /* (DATA page tables are built in PASS 2 after setup_sintran_window below,
     * so the bounded-PS_ADI builder's watermark pages land above the loader's
     * final allocation cursor.) */

    /* ========================================================================
     * SEGMENT 31: SINTRAN III Monitor Call Interception + Data Window
     * ========================================================================
     *
     * Segment 31 is reserved by SINTRAN III for MON (monitor) calls. When a
     * program executes CALL or CALLG to an address in segment 31 (0xF8xxxxxx),
     * the CPU should trap to the operating system to handle the request.
     *
     * In the emulator, we intercept these calls using the PC_IND (indirect
     * segment) capability flag. When PC_IND is set in a segment's program
     * capability, the CALL instruction does NOT jump to the target address.
     * Instead, nd500_check_indirect_call() in nd500_indirect.c intercepts
     * the call and dispatches it to the libmon MON call handler.
     *
     * Capability format for segment 31: 0x801F
     *   - Bit 15 (PC_IND = 0x8000): Indirect segment flag - triggers interception
     *   - Bits 13-5: Target domain (0 = kernel domain)
     *   - Bits 4-0: Target segment (31 = SINTRAN segment)
     *
     * When a program calls MON 11B (GetBasicTime), for example:
     *   1. Program executes: CALL 0xF8000009  (segment 31, offset 9 = MON 11B)
     *   2. CPU checks PC[31] capability, sees PC_IND flag is set
     *   3. nd500_check_indirect_call() intercepts the call
     *   4. MON number extracted from offset (9 = 0x09 = 11 octal)
     *   5. libmon dispatches to mon_11B_GetBasicTime() handler
     *   6. Handler executes, sets output parameters
     *   7. Control returns to instruction after CALL
     *
     * SINTRAN WINDOW (DC[31]):
     * In addition to MON call interception, segment 31 also serves as a
     * "SINTRAN Window" for accessing system data structures like RT descriptions.
     * MON 30B (GetOwnRTAddress) returns addresses in segment 31 (0xF8xxxxxx),
     * and programs may read/write data at these addresses.
     *
     * We allocate physical memory for segment 31 and set up DC[31] so that
     * data accesses to 0xF8xxxxxx are translated to this physical region.
     * This simulates the ND-100's DPIT (Data PIT) where SINTRAN stores
     * RT descriptions and other system tables.
     *
     * Without DC[31] setup, data access to segment 31 would cause an MMU fault.
     * ======================================================================== */

    /* PC[31]: Indirect segment for MON call interception */
    uint16_t pc31 = PC_IND | (0 << 5) | 31;  /* 0x801F: indirect to domain 0 segment 31 */
    nd500_mmu_set_program_capability(cpu, domain, 31, pc31);

    /* DC[31]: SINTRAN Window - allocate physical memory for system data
     * See setup_sintran_window() for architectural details and future ND-100 integration notes.
     */
    if (setup_sintran_window(m, cpu, domain, &next_psn, log_callback, log_context) != 0) {
        if (log_callback) {
            log_callback(log_context, "Warning: Failed to setup SINTRAN window (DC[31])");
        }
    }

    /* ========================================================================
     * PASS 2: build the DATA page tables (bounded PS_ADI two-level).
     * Adopts the loaded initialized pages, eagerly maps
     * DATA_GROWTH_RESERVE_PAGES of zeroed reserve for stack/heap/bss, and does
     * NOT register the segment growable - past the owned extent the MMU traps
     * to the guest THA exactly like the old PS_ASI mapping (NC's fault
     * boundary), but the owned extent may now exceed 512 pages / 1 MB.
     * Design: PLAN-nd500x-growable-DATA-option2-redesign-2026-07-26.md (b).
     * ======================================================================== */
    for (int i = 0; i < max_segs; i++) {
        if (seg_info[i].seg_type == SEG_TYPE_UNUSED || !seg_info[i].has_data) {
            continue;
        }
        uint32_t data_pages = (seg_info[i].data_size + 2047) / 2048;
        if (data_pages == 0) data_pages = 1;

        uint32_t l1_pfn = nd500_segment_map_bounded_data(cpu, m,
            seg_info[i].psn_data, seg_info[i].data_phys_base, data_pages,
            DATA_GROWTH_RESERVE_PAGES);
        if (l1_pfn == 0) {
            if (log_callback) {
                log_callback(log_context, "ERROR: bounded DATA mapping failed for seg %d", i);
            }
            return -1;
        }
        if (log_callback) {
            log_callback(log_context, "  Seg %d DATA: %u pages + %u reserve, PS_ADI L1 @ PFN %u, PSN %d",
                         i, data_pages, (uint32_t)DATA_GROWTH_RESERVE_PAGES,
                         l1_pfn, seg_info[i].psn_data);
        }
    }

    /* Enable MMU */
    nd500_mmu_enable_program(cpu);
    nd500_mmu_enable_data(cpu);

    if (log_callback) {
        log_callback(log_context, "");
        log_callback(log_context, "MMU Configuration:");
        log_callback(log_context, "  Highest PSN claimed: %d", next_psn - 1);
        log_callback(log_context, "  Segment 31: SINTRAN MON calls (indirect)");
        log_callback(log_context, "  MMU enabled (Program and Data)");
        if (tha != 0) {
            log_callback(log_context, "  THA: 0x%08X", tha);
        }
        log_callback(log_context, "");
        log_callback(log_context, "Physical pages still free: %u of %u",
                     nd500_phys_pages_free(m), nd500_phys_pages_total(m));
    }

    /* Set PC to start address */
    if (start_addr != 0) {
        cpu->PC = start_addr;
        if (log_callback) {
            log_callback(log_context, "PC set to start address: 0x%08X", start_addr);
        }
    }

    /* Register domain in debugger tracking system */
    {
        const char* filepath = ndlib_get_dom_filepath();
        /* Extract domain name from filename (basename without extension) */
        const char* base = filepath ? filepath : "unknown";
        const char* p;
        for (p = base; *p; p++) {
            if (*p == '/' || *p == '\\') base = p + 1;
        }
        char domain_name[64];
        strncpy(domain_name, base, sizeof(domain_name) - 1);
        domain_name[sizeof(domain_name) - 1] = '\0';
        /* Remove extension */
        for (int i = (int)strlen(domain_name) - 1; i >= 0; i--) {
            if (domain_name[i] == '.') {
                domain_name[i] = '\0';
                break;
            }
        }

        int seg_count = ndlib_dom_get_segment_count();
        nd500_debugger_register_domain((uint8_t)domain, domain_name, filepath, start_addr, tha, seg_count);
    }

    return 0;
}

/*
 * Simple printf-based log callback for command line use.
 * Ignores context, just prints to stdout.
 */
void ndlib_dom_log_printf(void* ctx, const char* fmt, ...) {
    (void)ctx;
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}
