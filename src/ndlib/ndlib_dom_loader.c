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
    /* Start at page 1 (0x800) to avoid PFN 0 which is used as "invalid PTE" marker */
    uint32_t phys_base = 0x00000800;  /* Page 1 = physical address 0x800 (2KB) */
    uint32_t total_loaded = 0;

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

        /* Calculate base physical address for this segment (page-aligned) */
        uint32_t seg_phys_base = (phys_base + total_loaded + 0x7FF) & ~0x7FFu;
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

        /* Update total loaded: round up to page boundary for next segment */
        total_loaded = (seg_phys_base - phys_base) + ((byte_offset + 2047) & ~2047u);
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

    /* ========================================================================
     * Set up MMU page tables using PS_ASI (single-level paging)
     *
     * For each segment with content, create a separate PST entry and page table.
     * ProgramAndData segments get TWO PST entries: one for PROG, one for DATA.
     * This matches the C# RetroCore implementation.
     * ======================================================================== */

    /* Track where to allocate page tables (after all segment data) */
    uint32_t pt_alloc_base = (phys_base + total_loaded + 2047) & ~2047u;
    int next_psn = 100;  /* Start allocating PST entries from 100 */

    /* Process each segment - create page tables and PST entries */
    for (int i = 0; i < max_segs; i++) {
        if (seg_info[i].seg_type == SEG_TYPE_UNUSED) {
            continue;
        }

        /* Create page table and PST entry for PROG (if segment has program) */
        if (seg_info[i].has_prog) {
            uint32_t prog_pages = (seg_info[i].prog_size + 2047) / 2048;
            if (prog_pages == 0) prog_pages = 1;

            /* Allocate page table */
            uint32_t pt_base = pt_alloc_base;
            pt_alloc_base = (pt_alloc_base + prog_pages * 4 + 2047) & ~2047u;

            /* Fill page table - PTEs for PROG pages */
            /* PTE format: [31:2]=PFN, [1]=unused, [0]=protection (0=RW, 1=RO) */
            for (uint32_t p = 0; p < prog_pages; p++) {
                uint32_t pte_addr = pt_base + p * 4;
                uint32_t pfn = (seg_info[i].prog_phys_base >> 11) + p;
                uint32_t pte = (pfn << 2) | 1;  /* protection=1 for code */
                nd500_bus_write32(m, pte_addr, pte);
            }

            /* Allocate PST entry and set up */
            seg_info[i].psn_prog = next_psn++;
            nd500_mmu_set_pst_entry(cpu, seg_info[i].psn_prog, PS_ASI, pt_base >> 11);

            /* Set program capability for this segment */
            nd500_mmu_set_program_capability(cpu, domain, i, seg_info[i].psn_prog | PC_DIR);

            if (log_callback) {
                log_callback(log_context, "  Seg %d PROG: %u pages, PT @ 0x%08X, PSN %d",
                             i, prog_pages, pt_base, seg_info[i].psn_prog);
            }
        }

        /* Create SEPARATE page table and PST entry for DATA (if segment has data) */
        if (seg_info[i].has_data) {
            uint32_t data_pages = (seg_info[i].data_size + 2047) / 2048;
            if (data_pages == 0) data_pages = 1;

            /* Allocate page table */
            uint32_t pt_base = pt_alloc_base;
            pt_alloc_base = (pt_alloc_base + data_pages * 4 + 2047) & ~2047u;

            /* Fill page table - PTEs for DATA pages
             * Page table maps to where DATA was actually copied (data_phys_base)
             */
            uint32_t data_base_pfn = seg_info[i].data_phys_base >> 11;

            for (uint32_t p = 0; p < data_pages; p++) {
                uint32_t pte_addr = pt_base + p * 4;
                uint32_t pfn = data_base_pfn + p;
                uint32_t pte = (pfn << 2) | 0;  /* protection=0 for RW data */
                nd500_bus_write32(m, pte_addr, pte);
            }

            /* Allocate PST entry and set up */
            seg_info[i].psn_data = next_psn++;
            nd500_mmu_set_pst_entry(cpu, seg_info[i].psn_data, PS_ASI, pt_base >> 11);

            /* Set data capability for this segment */
            nd500_mmu_set_data_capability(cpu, domain, i, seg_info[i].psn_data | DC_WRP);

            if (log_callback) {
                log_callback(log_context, "  Seg %d DATA: %u pages, PT @ 0x%08X, PSN %d",
                             i, data_pages, pt_base, seg_info[i].psn_data);
            }
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

    /* ========================================================================
     * SEGMENT 31: SINTRAN III Monitor Call Interception
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
     * Without this setup, CALL to segment 31 would cause an MMU fault because
     * there is no physical memory mapped to segment 31.
     * ======================================================================== */
    uint16_t pc31 = PC_IND | (0 << 5) | 31;  /* 0x801F: indirect to domain 0 segment 31 */
    nd500_mmu_set_program_capability(cpu, domain, 31, pc31);

    /* Enable MMU */
    nd500_mmu_enable_program(cpu);
    nd500_mmu_enable_data(cpu);

    if (log_callback) {
        log_callback(log_context, "");
        log_callback(log_context, "MMU Configuration:");
        log_callback(log_context, "  PSN range: 100-%d", next_psn - 1);
        log_callback(log_context, "  Segment 31: SINTRAN MON calls (indirect)");
        log_callback(log_context, "  MMU enabled (Program and Data)");
        if (tha != 0) {
            log_callback(log_context, "  THA: 0x%08X", tha);
        }
        log_callback(log_context, "");
        log_callback(log_context, "Total loaded: %u bytes", total_loaded);
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
