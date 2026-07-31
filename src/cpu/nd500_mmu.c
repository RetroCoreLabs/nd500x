#include "nd500_mmu.h"

/* The MMU's refusal messages must honour the MMU LOG LEVEL, not the instruction
 * trace mode. They used to use TRACE(), which is gated on
 * nd500_dbg_get_trace_mode() - so "show mmu errors" was silent and the only way
 * to learn WHY a translation was refused was to turn on full instruction
 * tracing. There are four distinct protect-violation causes and they are
 * indistinguishable from the outside; this is the level that tells them apart. */
#define MMU_ERR(...) do { \
    if (nd500_dbg_get_mmu_log_level() >= MMU_LOG_ERRORS) fprintf(stderr, __VA_ARGS__); \
} while (0)
#include "cpu_protos.h"
#include "../machine/machine_protos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Once-latched env flag: getenv() on the CPU run path races readline's
 * setenv (environ realloc) on the main thread -> SIGSEGV. Latch once. */
static int nd_env_flag(const char* name, int* latch) {
    if (*latch < 0) { const char* e = getenv(name); *latch = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return *latch;
}
static int g_envf_pst47dbg = -1;
static int g_envf_seg30dbg = -1;


/* Physical base where the flat a.out loader placed the DATA section (= a_text).
 * Declared here (not via ndlib.h) to keep the MMU free of a loader header
 * dependency. Used to de-alias I-space and D-space in the segment-0 identity
 * fallback below. [I/D-space fix] */
extern uint32_t ndlib_aout_get_data_base(void);

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

/* When enabled (ND500X_MMU_GUEST_TABLES=1), translate() reads capabilities/PST
 * entries from the guest's REAL in-memory tables at DITBASE/PSTP instead of the
 * emulator shadow arrays - the architecturally-correct behaviour that lets the
 * kernel's runtime table edits (kpcbinit, __resume u-area remap, newproc) take
 * effect. Still WIP: it shifts demand-paging/page-fault handling to the kernel
 * (via the PGF trap + THA), which needs the trap/u-area path to work. Default
 * OFF so the current boot (mounts root) is unchanged. */
static int g_mmu_guest_tables = -1;
static int mmu_use_guest_tables(void) {
    if (g_mmu_guest_tables < 0) {
        const char* e = getenv("ND500X_MMU_GUEST_TABLES");
        g_mmu_guest_tables = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return g_mmu_guest_tables;
}

/* Segment-level demand mapping: when a DATA access references a work segment
 * that has no capability, allocate a backed (PS_ADI, demand-grown) segment on
 * the fly, mirroring how SINTRAN maps scratch segments on first use. The NC C
 * compiler's code generator references more work-segments (2..6) than it
 * explicitly allocates via GSWSP, and relies on this. Bounded to a plausible
 * work-segment range and gated so genuinely-wild accesses still trap. */
extern int nd500_mon_allocate_segment(void* cpu, void* machine, uint8_t domain,
    uint32_t requested_segment, uint32_t segment_size_bytes,
    uint32_t* out_assigned_segment);
#define DEMAND_SEG_MIN_SEGMENT   1       /* 0=alias (identity-backed image); 1..30 demand-backed; 31=SINTRAN window */
#define DEMAND_SEG_MAX_SEGMENT   30    /* incl. 29=_Kstack/u-area, 30=UDATA; 31=SINTRAN window stays special */
#define DEMAND_SEG_INIT_BYTES    (128u*1024u)  /* grows on demand beyond this */
static int mmu_demand_segments = -1;    /* -1 = read env once; default ON */

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
    if (!nd500_quiet) printf("ND-500: Data MMU enabled (DMON)\n");
}

void nd500_mmu_disable_data(Nd500Cpu* cpu) {
    if (!cpu) return;
    g_mmu_data_enabled = 0;
    /* Disable machine mmu_enabled only if both program AND data MMU are disabled */
    if (cpu->machine && !g_mmu_program_enabled) cpu->machine->mmu_enabled = 0;
    if (!nd500_quiet) printf("ND-500: Data MMU disabled (DMOF)\n");
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
    if (!nd500_quiet) printf("ND-500: Program MMU enabled (PMON)\n");
}

void nd500_mmu_disable_program(Nd500Cpu* cpu) {
    if (!cpu) return;
    g_mmu_program_enabled = 0;
    /* Disable machine mmu_enabled only if both program AND data MMU are disabled */
    if (cpu->machine && !g_mmu_data_enabled) cpu->machine->mmu_enabled = 0;
    if (!nd500_quiet) printf("ND-500: Program MMU disabled (PMOF)\n");
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
/**
 * Core MMU translation with explicit domain parameter.
 *
 * Domain selection per ND-500 Reference Manual:
 * - Instruction fetch: always uses CED (Current Executing Domain)
 * - Data access without ALT: uses CED
 * - Data access with ALT prefix: uses CAD (Current Alternative Domain)
 *
 * The ALT prefix allows called routines to access caller's data when
 * crossing domain boundaries.
 */
uint32_t nd500_mmu_translate_domain(Nd500Cpu* cpu, uint32_t virtual_addr, int is_write, int is_instruction, uint8_t domain) {
    /* Debug: trace all translations for high addresses */
    if (nd500_dbg_get_mmu_log_level() >= MMU_LOG_TRACE && virtual_addr >= 0x08000000 && is_write) {
        fprintf(stderr, "[MMU-TRACE] translate(vaddr=0x%08X, is_write=%d, is_instr=%d, domain=%d)\n",
                virtual_addr, is_write, is_instruction, domain);
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
            if (nd500_dbg_get_mmu_log_level() >= MMU_LOG_ERRORS && virtual_addr >= 0x08000000) {
                fprintf(stderr, "[MMU] Data MMU DISABLED! vaddr=0x%08X returned unchanged (DMON not executed?)\n", virtual_addr);
            }
            return virtual_addr;  /* Data MMU disabled - direct physical addressing */
        }
    }

    /* Sanity check tables */
    if (!g_pst || !g_pcb_table) {
        if (nd500_dbg_get_mmu_log_level() >= MMU_LOG_ERRORS) {
            fprintf(stderr, "[MMU] Tables not initialized! PST=%p PCB=%p\n", (void*)g_pst, (void*)g_pcb_table);
        }
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

    /* Domain parameter is now passed explicitly - no need to read from cpu->CAD */
    /* Note: domain is uint8_t (0-255), MAXDOM is 256, so domain < MAXDOM is always true */

    /* Read the guest's REAL tables ONLY for the per-process segments that
     * __resume remaps (locore.c:920): 26=_Utext, 29=_u/Kstack, 30=_Udata,
     * 31=_Ustack. This makes the u-area remap (and thus per-process context
     * switch / u.u_procp) resolve correctly - the fix for `panic: sleep` -
     * while the kernel's self-referential phys-map bootstrap (seg 2) and the
     * other kernel segments stay on the emulator's proven management, avoiding
     * the early page-fault-during-bootstrap problem. Full guest-table mode
     * (all segments) remains available but needs the PGF->kernel dispatch. */
    int use_guest = mmu_use_guest_tables() && cpu->machine && cpu->DITBASE
                 && (/* User domains (domain != KDOM=0) have NO direct-loaded image:
                      * every segment of a user process is mapped only by the guest
                      * capability tables (pcbfork sets pcb_pc[0]/pcb_dc[0]/stack etc.,
                      * the icode is placed by vmemall+copyiout into proc[1]'s real
                      * physical text page). The kernel's selective set below covers
                      * only domain 0, whose low segments (0=ktext,1) are the flat
                      * direct-loaded kernel image. So for domain != 0, route ALL
                      * segments through the guest DIT/PST. Without this the /etc/init
                      * launch fetches domain-1 seg-0 VA=4 through the emulator's stale
                      * demo shadow (mmusetup) at physical 0x80000 (empty) -> 0x00. */
                     domain != 0
                     || segment == 26 || segment == 29 || segment == 30 || segment == 31
                     /* Page-table window segments the kernel manages recursively:
                      * 3=_usrpi1 (0x18000000), 4=_usrpt (0x20000000), 5=_susrpt
                      * (0x28000000). vgetpt writes new-process u-area/data PTEs
                      * through usrpt (seg 4) via Usrptmap; the flat shadow mapping
                      * sent those writes to the wrong physical page, so Pst[38]'s
                      * page table stayed empty and __resume page-faulted. Routing
                      * these through the guest tables makes PTE writes/reads land
                      * where the PST entries point. */
                     || segment == 3 || segment == 4 || segment == 5
                     /* 7 = the no-cache segment (NO_CACHE_SEG_START 0x38000000,
                      * machine/param.h): the kernel maps the DISK BUFFER pool
                      * here (machdep startup, ncsize += MAXBSIZE*nbuf) with its
                      * own PTEs. Through the shadow tables the buffer window
                      * diverged from the kernel's mapping after exec recycled
                      * buffers: namei's geteblk name buffer and dirlookup's
                      * bread buffers read back stale/garbage bytes, so EVERY
                      * post-exec lookup died with "/: bad dir ino 2 at offset
                      * 0: mangled entry" -> ENOENT. */
                     || segment == 7
                     /* 2 = Physbase (_Physbase, virtual 0x10000000, DC_PHYS). The
                      * kernel builds seg-2 as a self-referential IDENTITY map of all
                      * physical memory (machdep.c startup: PS_AZI->PS_ASI->PS_ADI,
                      * pte->pg_pfnum = i). It writes the ADI page-table PAGES *through
                      * Physbase itself*, and sets Pst[physindex]/DIT[dom0 seg2] via the
                      * seg 27/28 windows onto PSTP/DITBASE. If seg-2 translates through
                      * the emulator SHADOW tables instead, those self-referential
                      * writes land in demand-allocated pages (a fixed page skew), so a
                      * later usrpt L1 PTE the kernel wrote via Physbase reads back 0 and
                      * page-faults. Routing seg-2 through the guest tables (like the
                      * hardware, which has no shadow) makes the identity map coincide
                      * with raw physical memory: kernel-pfnum P == physical page P. */
                     || segment == 2);

    /* Get capability by reading the guest's REAL Domain Information Table at
     * DITBASE (like the hardware): DIT stride 256 bytes/domain; program table at
     * +0, data table at +64; each capability a 16-bit big-endian halfword indexed
     * by segment*2. This is what makes the kernel's runtime capability edits
     * (kpcbinit, __resume u-area remap, newproc) take effect. mmusetup mirrors its
     * initial setup and the demand allocator its segments into this same table. */
    uint16_t capability;
    if (use_guest) {
        uint32_t cap_addr = cpu->DITBASE + (uint32_t)domain * 256u
                          + (is_instruction ? 0u : 64u) + (uint32_t)segment * 2u;
        capability = (uint16_t)(((uint32_t)nd500_bus_read8(cpu->machine, cap_addr) << 8)
                              |  (uint32_t)nd500_bus_read8(cpu->machine, cap_addr + 1));
    } else {
        /* Emulator-side shadow tables (default). */
        capability = is_instruction
            ? g_pcb_table[domain].program_capabilities[segment]
            : g_pcb_table[domain].data_capabilities[segment];
    }

    if (nd_env_flag("ND500X_SEG30DBG", &g_envf_seg30dbg) && domain == 0 && segment == 30 && !is_instruction) {
        static uint64_t n = 0;
        if (n++ < 12)
            fprintf(stderr, "[SEG30DBG] dom0 seg30 vaddr=0x%08X use_guest=%d cap=0x%04X (PSN=%u ind=%d) @PC=0x%08X\n",
                    virtual_addr, use_guest, capability, capability & 0x7FF,
                    (capability & 0x8000) ? 1 : 0, cpu->PC);
    }

    /* Kernel DATA-segment-aliased-as-segment-1 (domain 0). The NDIX kernel builds
     * its syscall Start Address Vector + low-level _domain_call code into its DATA
     * segment at offset 0 and maps that data segment as SEGMENT 1 (0x08000000) so the
     * user seg-31 syscall gate (PC_IND|1 -> dom0 seg1) resolves through it (locore.c
     * ZERO: SAV[0]=1, SAV[1]=_domain_call+0x08000000). The emulator's shadow/mmusetup
     * capability for dom0 seg1 points elsewhere (empty), so force the alias: dom0 seg1
     * offset X -> physical data_base + X, for BOTH the SAV data read AND the
     * _domain_call program fetch. This is the same physical data the kernel reads via
     * seg-0 (data_base+X), so it is consistent. [syscall seg1 alias] */
    if (domain == 0 && segment == 1 && cpu->machine) {
        uint32_t data_base = ndlib_aout_get_data_base();
        if (data_base != 0) {
            uint32_t phys = data_base + (virtual_addr & 0x07FFFFFFu);
            if (phys < cpu->machine->memory_size) return phys;
        }
    }

    /* Check if capability is valid (non-zero) */
    if (capability == 0) {
        /* Null-pointer DATA read/write (effective address == 0) is NOT a protect
         * violation on real ND-500. Per the ND-500 Reference Manual (ND-05.009.4
         * p73): "An address equal to zero will cause an Address Zero trap
         * condition." AZ (status bit 24) is IGNORABLE (p63): if it is not enabled
         * in the domain's Own Trap Enable, the condition is merely recorded and
         * execution continues - the access completes. Real ND software relies on
         * this: the ND Linker's HELP command sorts its command table and, for an
         * empty command slot, dereferences a null name pointer (reads data VA 0);
         * that domain has AZ disabled (OTE bit 24 clear) so real hardware
         * continues. raise_trap() with an ignorable, OTE-disabled bit only sets
         * the status bit and returns without stopping. We map the access to
         * physical 0 (the unused low page: reads as 0, writes are discarded). See
         * docs/HELP-CRASH-ADVANCED-CMD-REGISTRATION.md.
         *
         * NOTE: an absent capability for a NON-zero address remains handled as a
         * protect violation below (unchanged); the manual-correct trap there is a
         * page fault, tracked as a separate cleanup, not applied here. */
        if (!is_instruction && virtual_addr == 0) {
            raise_trap(cpu, TRAP_AZ, cpu->PC, virtual_addr);
            return 0;
        }
        /* Segment-level demand mapping (data work-segments only). SINTRAN maps a
         * scratch segment on first use; NC codegen touches work-segments it did
         * not explicitly GSWSP-allocate and relies on this. Allocate a backed
         * PS_ADI segment for this segment number, then re-read the capability and
         * fall through to translate. Bounded + logged so wild pointers to other
         * segments still trap. */
        if (mmu_demand_segments < 0) {
            const char* e = getenv("ND500X_NO_DEMAND_SEGMENTS");
            mmu_demand_segments = (e && e[0] && e[0] != '0') ? 0 : 1;
        }
        if (!is_instruction && mmu_demand_segments && cpu->machine &&
            /* KERNEL DOMAIN ONLY. User-domain (domain != 0) segments are
             * managed exclusively by the NDIX kernel's paging - a missing
             * capability there is a WILD POINTER and must fault to the
             * kernel (pagein -> SIGSEGV), not be silently backed. During the
             * ls -l crash the allocator quietly mapped 'data segment 13
             * (domain 4, vaddr=0x6C656182)' - ASCII garbage as an address -
             * masking the real corruption. Domain 0 keeps demand mapping
             * (seg 29 u-area & friends rely on it before the kernel tables
             * exist), as does single-domain SINTRAN (CED always 0). */
            domain == 0 &&
            segment >= DEMAND_SEG_MIN_SEGMENT && segment <= DEMAND_SEG_MAX_SEGMENT) {
            uint32_t assigned = 0;
            int rc = nd500_mon_allocate_segment(cpu, cpu->machine, domain,
                        (uint32_t)segment, DEMAND_SEG_INIT_BYTES, &assigned);
            if (rc == 0) {
                capability = g_pcb_table[domain].data_capabilities[segment];
                if (!nd500_quiet)
                    printf("ND-500: demand-mapped data segment %d (domain %d, vaddr=0x%08X)\n",
                           segment, domain, virtual_addr);

                /* Publish the kernel u-area in the PST slot NDIX expects.
                 *
                 * On real hardware the ND-100 loads the kernel and fills the
                 * segment table entries Pst[first_phys_seg+1 .. +12] (machdep.c
                 * "the 100 has loaded the kernel ... left us with a single
                 * contiguous area"). NDIX only DERIVES those indices - machdep.c:184
                 * computes stackindex and never assigns it - and
                 * sys/init_main.c:74 then makes proc0's u-area
                 *     p_addr = Pst[FIRST_PHYS_SEG].ps_pfnum + STACKINDEX
                 * nd500x has no ND-100 loader, so that slot kept mmusetup's identity
                 * value (pfn == psn) and resolved to kernel low memory. _resume
                 * (locore.c:960-970) swaps segment 29's capability for
                 * pstindex|0x8000, so it read B/L out of phys 0x8000 and its `retd`
                 * jumped to garbage - the intermittent 0x4700204D halt.
                 *
                 * Segment 29 IS the kernel u-area (`_u` at 0xE8000000), so once it is
                 * backed here, alias the same PST entry into Pst[stackindex]. Both
                 * the normal demand-backed capability and _resume's PST lookup then
                 * reach the same physical pages. */
                if (segment == 29 && cpu->PSTP) {
                    uint32_t fps = nd500_bus_read32(cpu->machine,
                                       cpu->PSTP + (uint32_t)FIRST_PHYS_SEG * 4u)
                                   & 0x3FFFFFFFu;
                    uint32_t stack_psn = fps + STACKINDEX;
                    uint32_t alloc_psn = (uint32_t)(capability & DC_PSN);
                    if (stack_psn && stack_psn < MAX_PST && alloc_psn < MAX_PST) {
                        uint32_t w = nd500_bus_read32(cpu->machine,
                                         cpu->PSTP + alloc_psn * 4u);
                        nd500_mmu_set_pst_entry(cpu, (int)stack_psn,
                                                (uint8_t)(w >> 30),
                                                w & 0x3FFFFFFFu);
                        if (!nd500_quiet)
                            printf("ND-500: kernel u-area published to PST[%u] "
                                   "(first_phys_seg=%u) from PSN %u = 0x%08X\n",
                                   stack_psn, fps, alloc_psn, w);
                    }
                }
            }
        }
        if (capability == 0) {
            /* Directly-loaded kernel image fallback (identity mapping).
             *
             * The NDIX kernel is loaded flat into low physical memory (text at 0x0,
             * plus its rodata/const pool). Those low-segment (0/1) DATA accesses have
             * no PCB capability because firmware/SINTRAN never set one up in this
             * emulator - on real hardware they are covered by a direct (PS_AZI)
             * kernel-text/data capability installed at boot.
             *
             * When the referenced virtual address already lies inside the physical
             * RAM we hold the loaded image in, map it identity (virtual == physical)
             * instead of trapping. This mirrors the pre-MMU identity behaviour the
             * emulator relied on for low addresses, while high kernel segments (e.g.
             * segment 29 = _Kstack/u-area at 0xE8000000, far beyond physical RAM)
             * still go through the demand-segment allocator above. Without this, a
             * legitimate read of a kernel constant (e.g. vaddr 0x00022924) would
             * spuriously protect-fault the moment the data MMU is enabled. */
            if (cpu->machine && virtual_addr < cpu->machine->memory_size) {
                /* Separate I-space / D-space de-aliasing. The flat a.out loader
                 * places TEXT at physical 0 and DATA at physical data_base
                 * (= a_text). A DATA access (is_instruction == 0) to segment-0
                 * virtual V must therefore target physical (data_base + V) - the
                 * D-space image - while a program fetch stays identity (V, the
                 * I-space text). Without this, data reads of a text-range virtual
                 * address return code bytes instead of the intended data, which
                 * surfaces as garbage pointers (e.g. 0xFC16C51C in _strlen).
                 * Falls back to identity when no a.out is loaded (data_base == 0)
                 * or the offset would leave physical memory. [I/D-space fix] */
                if (!is_instruction) {
                    uint32_t data_base = ndlib_aout_get_data_base();
                    if (data_base != 0) {
                        uint32_t phys = virtual_addr + data_base;
                        if (phys < cpu->machine->memory_size) {
                            return phys;
                        }
                    }
                }
                return virtual_addr;
            }
            MMU_ERR("[MMU] TRAP: No %s capability! domain=%d segment=%d vaddr=0x%08X\n",
                  is_instruction ? "program" : "data", domain, segment, virtual_addr);
            /* "Zero in the capability" - NOT a write protect violation. The NDIX
             * T_PV handler only attempts pagein() for PVWVIOL, so reporting the
             * truth here keeps a zero capability on the panic/SIGSEGV path where
             * it belongs (machine/trap.c: "the capability ... is zero" is listed
             * as a separate cause from a write protected page). */
            cpu->mmu_pgf_where = MMW_ZEROCAP | (is_instruction ? MMW_INST : 0u);
            trap_protect_violation(cpu, cpu->PC, virtual_addr);
            return virtual_addr;  /* Return virtual address, trap will stop execution */
        }
    }

    /* ─────────────────────────────────────────────────────────
     * LEVEL 2: Capability → PST Entry
     * ───────────────────────────────────────────────────────── */

    /* An INDIRECT program capability is not a PSN. Bit 15 of a PROGRAM capability
     * is PC_IND: the remaining bits are a target domain (PC_DOM) and segment
     * (PC_SEG) to be resolved by the CALL/CALLG indirect dispatch
     * (nd500_indirect.c), NOT a physical segment number. Reaching the translate
     * path with one set means an ordinary fetch is running through a gate
     * capability - the call resolution did not happen. Masking it with PC_PSN and
     * walking the PST is a garbage walk into whatever entry the domain/segment
     * bits happen to spell. The hardware traps, and the NDIX kernel names the two
     * cases in machine/trap.c mmtraptype[]: 6 = "Indirect capability to another
     * machine" (PC_OMC set, the SINTRAN/ND-100 side), 7 = "Indirect capability
     * within the machine".
     *
     * DATA capabilities are excluded on purpose: bit 15 of a data capability is
     * DC_WRP (write permitted), not an indirect-type bit, so a writable data
     * segment must not be diverted here. */
    if (is_instruction && (capability & PC_IND)) {
        MMU_ERR("[MMU] TRAP: indirect program capability on a plain fetch! "
                "domain=%d segment=%d cap=0x%04X vaddr=0x%08X\n",
                domain, segment, capability, virtual_addr);
        cpu->mmu_pgf_where = ((capability & PC_OMC) ? MMW_IND_OTHER : MMW_IND_SAME)
                           | MMW_INST;
        trap_protect_violation(cpu, cpu->PC, virtual_addr);
        return virtual_addr;
    }

    /* Extract PSN (Physical Segment Number) from capability */
    int psn = capability & PC_PSN;  /* Lower 13 bits */

    if (psn >= MAX_PST) {
        MMU_ERR("[MMU] TRAP: PSN %d >= MAX_PST %d! vaddr=0x%08X\n", psn, MAX_PST, virtual_addr);
        cpu->mmu_pgf_where = MMW_INDEXERR | (is_instruction ? MMW_INST : 0u);
        trap_protect_violation(cpu, cpu->PC, virtual_addr);
        return virtual_addr;  /* Invalid PSN - return virtual address, trap will stop execution */
    }

    /* Check write permission (for data writes only) */
    if (!is_instruction && is_write) {
        /* Check DC_WRP flag: DC_WRP SET = Write Permitted, DC_WRP CLEAR = Read-only */
        if (!(capability & DC_WRP)) {
            MMU_ERR("[MMU] TRAP: WRITE DENIED! segment=%d missing DC_WRP flag! cap=0x%04X vaddr=0x%08X\n",
                  segment, capability, virtual_addr);
            /* PVWVIOL, MMINST clear (a data write by definition): this is the ONE
             * protect violation the NDIX kernel tries to recover from - T_PV and
             * T_PV+USER both call pagein() for it and only signal when that fails
             * (machine/trap.c:314, :360). Leaving cx_info at 0 made every such
             * write panic("Kernel Protect Violation") or SIGSEGV outright. */
            cpu->mmu_pgf_where = MMW_PVWVIOL;
            trap_protect_violation(cpu, cpu->PC, virtual_addr);
            return virtual_addr;  /* Write to read-only segment - return virtual address, trap will stop execution */
        }
    }

    /* Get PST entry by reading the guest's REAL Physical Segment Table at PSTP
     * (struct pste, big-endian: ps_index@[31:30], ps_pfnum@[29:0]). The kernel
     * extends this table at runtime (newproc writes Pst[p_addr]); reading it here
     * is what lets __resume's u-area remap resolve to the new process. */
    PhysicalSegmentTableEntry pst_entry;
    if (use_guest && cpu->PSTP) {
        uint32_t pa = cpu->PSTP + (uint32_t)psn * 4u;
        uint32_t w = ((uint32_t)nd500_bus_read8(cpu->machine, pa)     << 24)
                   | ((uint32_t)nd500_bus_read8(cpu->machine, pa + 1) << 16)
                   | ((uint32_t)nd500_bus_read8(cpu->machine, pa + 2) << 8)
                   |  (uint32_t)nd500_bus_read8(cpu->machine, pa + 3);
        pst_entry.index_mode   = (uint8_t)(w >> 30);
        pst_entry.physical_pfn = w & 0x3FFFFFFF;
    } else {
        pst_entry = g_pst[psn];
    }

    /* A ZERO PST ENTRY IS A PAGE FAULT - not a direct mapping of physical page 0.
     * ND-05.009.4 section 4.3: "If the Physical Segment Table entry is 0, this means
     * that no mapping exists for the logical address that needs translation. This is
     * a page fault trap condition."  Without this a zero entry decodes as PS_AZI with
     * pfn 0 and silently translates to physical page 0, which is never mappable.
     * Mirrors CpuND500.MMU.cs ReadPstEntry/pstEntryIsZero. [PST zero entry 2026-07-27] */
    if (pst_entry.index_mode == PS_AZI && pst_entry.physical_pfn == 0) {
        MMU_ERR("[MMU] TRAP: PST entry %d is ZERO - no mapping exists! vaddr=0x%08X\n",
                psn, virtual_addr);
        {   /* branch tag (env ND500X_PGFDBG): identify WHICH mmu branch raised
             * the silent second _Udata fault (no PTWDBG-L2/PST47 line). */
            static int pgfd = -1;
            if (pgfd < 0) { const char* e = getenv("ND500X_PGFDBG"); pgfd = (e && e[0] && e[0] != '0') ? 1 : 0; }
            if (pgfd) fprintf(stderr, "[PGFSITE] PST-ZERO dom=%d seg=%d psn=%d va=0x%08X use_guest=%d\n",
                              domain, segment, psn, virtual_addr, use_guest);
        }
        trap_page_fault(cpu, cpu->PC, virtual_addr);
        return virtual_addr;
    }

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
                MMU_ERR("[MMU] TRAP: PS_AZI page fault! L1=%d L2=%d must be 0! vaddr=0x%08X\n",
                      l1_index, l2_index, virtual_addr);
                {   static int pgfd = -1;
                    if (pgfd < 0) { const char* e = getenv("ND500X_PGFDBG"); pgfd = (e && e[0] && e[0] != '0') ? 1 : 0; }
                    if (pgfd) fprintf(stderr, "[PGFSITE] AZI-IDX dom=%d seg=%d psn=%d va=0x%08X pfn=0x%X\n",
                                      domain, segment, psn, virtual_addr, pst_entry.physical_pfn);
                }
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
                MMU_ERR("[MMU] TRAP: PS_ASI page fault! L1=%d must be 0! vaddr=0x%08X\n",
                      l1_index, virtual_addr);
                {   static int pgfd = -1;
                    if (pgfd < 0) { const char* e = getenv("ND500X_PGFDBG"); pgfd = (e && e[0] && e[0] != '0') ? 1 : 0; }
                    if (pgfd) fprintf(stderr, "[PGFSITE] ASI-IDX dom=%d seg=%d psn=%d va=0x%08X\n",
                                      domain, segment, psn, virtual_addr);
                }
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
                MMU_ERR("[MMU] TRAP: PS_ASI page not valid! vaddr=0x%08X pte_addr=0x%08X\n", virtual_addr, pte_addr);
                {   static int pgfd = -1;
                    if (pgfd < 0) { const char* e = getenv("ND500X_PGFDBG"); pgfd = (e && e[0] && e[0] != '0') ? 1 : 0; }
                    if (pgfd) fprintf(stderr, "[PGFSITE] ASI-PTE dom=%d seg=%d psn=%d va=0x%08X pte@0x%08X\n",
                                      domain, segment, psn, virtual_addr, pte_addr);
                }
                trap_page_fault(cpu, cpu->PC, virtual_addr);
                return virtual_addr;  /* Page not mapped - return virtual address, trap will stop execution */
            }

            /* PTE bit 31 is DATA-PAGE write protection on the LAST indexing
             * level, and in PS_ASI the single level IS the last level.
             * ND-05.009.4 Figure 14: "Bit 31 in an index page table entry is
             * reserved except on the last indexing level. That is, when the
             * page number part of the entry specifies a data page, then bit 31
             * is used for data page write protection."
             * It was previously gated on is_instruction, so a data write to a
             * read-only page succeeded here while the PS_ADI branch below
             * correctly rejected it - the two paths disagreed. The old comment
             * justified this with "C# reference creates all PTEs with
             * protection=1", which is no longer true of that code. */
            if (is_write && pte.protection != 0) {
                MMU_ERR("[MMU] TRAP: Instruction write to read-only page! vaddr=0x%08X pte_addr=0x%08X prot=%d\n",
                      virtual_addr, pte_addr, pte.protection);
                /* PVWVIOL: a write-protected PAGE, the recoverable case (see the
                 * DC_WRP site above). MMINST stays clear - this is a data write. */
                cpu->mmu_pgf_where = MMW_PVWVIOL;
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
                /* A MON-connected segment (412B FSCNT / 422B GSWSP) is grown on
                 * demand, matching the manual's paged segment model - allocate
                 * the missing L2 table + page and re-read rather than trapping. */
                if (!is_instruction && nd500_segment_grow_on_fault(cpu, virtual_addr, domain)) {
                    l1_pte = nd500_mmu_read_pte(cpu, l1_pte_addr);
                }
            }
            if (!l1_pte.valid) {
                MMU_ERR("[MMU] TRAP: PS_ADI L1 page not valid! vaddr=0x%08X l1_pte_addr=0x%08X\n", virtual_addr, l1_pte_addr);
                /* PTWDBG: prove/refute the Physbase-linear-map (seg 2) round-trip.
                 * The kernel writes this very L1 table THROUGH the seg-2 linear map
                 * at virtual (Physbase + l1_table_base). If seg-2 maps that back to
                 * physical l1_table_base, the write and this read agree; if not, the
                 * kernel's PTE writes are landing on the wrong page - the real root. */
                {
                    const char* e = getenv("ND500X_PTWDBG");
                    if (e && e[0] && e[0] != '0') {
                        uint32_t phys_l1  = nd500_bus_read32(cpu->machine, l1_pte_addr);
                        uint32_t alias_va = 0x10000000u + l1_table_base; /* Physbase(seg2)+X */
                        /* Walk the GUEST seg-2 tables (DIT->PST->ADI) exactly as the
                         * CPU does - NOT nd500_mmu_peek (that reads the shadow tables). */
                        uint32_t s2 = (alias_va >> SGSHIFT) & 0x1F;
                        uint32_t s2_l1 = (alias_va >> L1_INDEX_SHIFT) & L1_INDEX_MASK;
                        uint32_t s2_l2 = (alias_va >> L2_INDEX_SHIFT) & L2_INDEX_MASK;
                        uint32_t s2_off = alias_va & (NBPG - 1);
                        uint32_t capA = cpu->DITBASE + 0u*256u + 64u + s2*2u;
                        uint16_t cap  = (uint16_t)((nd500_bus_read8(cpu->machine, capA) << 8)
                                                 |  nd500_bus_read8(cpu->machine, capA + 1));
                        uint32_t psn  = cap & PC_PSN;
                        uint32_t pstw = nd500_bus_read32(cpu->machine, cpu->PSTP + psn*4u);
                        uint32_t pmode = pstw >> 30, ppfn = pstw & 0x3FFFFFFF;
                        uint32_t l1w = nd500_bus_read32(cpu->machine, (ppfn<<PGSHIFT) + s2_l1*4u);
                        uint32_t l2base = (l1w & 0x3FFFFFFF);
                        uint32_t l2w = nd500_bus_read32(cpu->machine, (l2base<<PGSHIFT) + s2_l2*4u);
                        uint32_t gphys = ((l2w & 0x3FFFFFFF)<<PGSHIFT) + s2_off;
                        uint32_t gval  = nd500_bus_read32(cpu->machine, gphys);
                        fprintf(stderr, "[PTWDBG] seg=%d usrpt L1@phys0x%08X=0x%08X | seg2 GUEST-walk "
                                "va=0x%08X cap=0x%04X psn=%u mode=%u l1=0x%08X l2=0x%08X -> gphys=0x%08X val=0x%08X\n",
                                segment, l1_pte_addr, phys_l1, alias_va, cap, psn, pmode, l1w, l2w, gphys, gval);
                    }
                }
                /* PFZ1: zero 1st-level page-table entry. MMINST (0x40) marks an
                 * I-channel (instruction fetch) fault - the NDIX kernel derives
                 * the fault SPACE from it (trap.c T_PGF+USER: access=(info&
                 * MMINST)>>5; segno+access classifies text vs data). Without it
                 * a text-fetch fault at va 0 pages in DATA page 0 instead. */
                cpu->mmu_pgf_where = MMW_PFZ1 | (is_instruction ? MMW_INST : 0u);
                trap_page_fault(cpu, cpu->PC, virtual_addr);
                return virtual_addr;  /* L1 page table not present - return virtual address, trap will stop execution */
            }

            /* L1 PTE points to L2 page table */
            uint32_t l2_table_base = l1_pte.physical_pfn << PGSHIFT;
            uint32_t l2_pte_addr = l2_table_base + (l2_index * 4);

            /* Read L2 PTE */
            PageTableEntry l2_pte = nd500_mmu_read_pte(cpu, l2_pte_addr);

            /* PST47DBG: full walk chain for the shared user-data segment (PSN 47 =
             * icode p_addr+1). Shows whether PST[47] -> L1 -> L2 resolves to a page
             * or where the chain is empty (the _Udata / seg-30 boot blocker). */
            if (nd_env_flag("ND500X_PST47DBG", &g_envf_pst47dbg) && psn == 47) {
                static uint64_t n47 = 0;
                if (n47++ < 40) {
                    uint32_t l1w = nd500_bus_read32(cpu->machine, l1_pte_addr);
                    uint32_t l2w = nd500_bus_read32(cpu->machine, l2_pte_addr);
                    fprintf(stderr,
                        "[PST47] dom=%d seg=%d va=0x%08X L1i=%d L2i=%d PC=0x%08X | "
                        "PST47{mode=%u pfn=0x%X} l1@0x%08X=0x%08X(pfn0x%X v=%d) "
                        "l2base=0x%08X l2@0x%08X=0x%08X(v=%d)\n",
                        domain, segment, virtual_addr, l1_index, l2_index, cpu->PC,
                        pst_entry.index_mode, pst_entry.physical_pfn,
                        l1_pte_addr, l1w, l1_pte.physical_pfn, l1_pte.valid,
                        l2_table_base, l2_pte_addr, l2w, l2_pte.valid);
                }
            }

            if (!l2_pte.valid) {
                /* Same demand-growth path as the L1 miss above. */
                if (!is_instruction && nd500_segment_grow_on_fault(cpu, virtual_addr, domain)) {
                    l2_pte = nd500_mmu_read_pte(cpu, l2_pte_addr);
                }
            }
            if (!l2_pte.valid) {
                /* A zero L2 PTE is the ROUTINE demand-paging fault in a paging
                 * OS (every text/data page of every exec'd program) - log it
                 * only at TRACE, not at the default ERRORS level, or the
                 * console drowns during normal NDIX operation. */
                if (nd500_dbg_get_mmu_log_level() >= MMU_LOG_TRACE)
                    fprintf(stderr, "[MMU] TRAP: PS_ADI L2 page not valid! vaddr=0x%08X l2_pte_addr=0x%08X\n", virtual_addr, l2_pte_addr);
                {
                    const char* e = getenv("ND500X_PTWDBG");
                    if (e && e[0] && e[0] != '0') {
                        fprintf(stderr, "[PTWDBG-L2] seg=%d va=0x%08X psn=%d pst_pfn=0x%X l1_pte@0x%08X=pfn0x%X "
                                "l2_pte_addr=0x%08X raw=0x%08X\n", segment, virtual_addr, psn,
                                pst_entry.physical_pfn, l1_pte_addr, l1_pte.physical_pfn, l2_pte_addr,
                                nd500_bus_read32(cpu->machine, l2_pte_addr));
                        fprintf(stderr, "[PTWDBG-L2] guest PST[13..21]:");
                        for (int q = 13; q <= 21; q++)
                            fprintf(stderr, " [%d]=0x%08X", q, nd500_bus_read32(cpu->machine, cpu->PSTP + q*4u));
                        fprintf(stderr, "\n[PTWDBG-L2] guest DIT dom0 seg2 data@0x%08X=0x%04X\n",
                                cpu->DITBASE + 64u + 2u*2u,
                                (nd500_bus_read8(cpu->machine, cpu->DITBASE+64u+4u)<<8)
                                | nd500_bus_read8(cpu->machine, cpu->DITBASE+64u+5u));
                    }
                }
                /* PFZ2: zero 2nd-level page-table entry (demand page). MMINST
                 * (0x40) marks an I-channel fault - see the PFZ1 site above. */
                cpu->mmu_pgf_where = MMW_PFZ2 | (is_instruction ? MMW_INST : 0u);
                trap_page_fault(cpu, cpu->PC, virtual_addr);
                return virtual_addr;  /* L2 page not mapped - return virtual address, trap will stop execution */
            }

            /* Write permission comes from the L2 entry only. ND-05.009.4
             * Figure 14: "Bit 31 in an index page table entry is reserved
             * except on the last indexing level." The L1 entry addresses
             * another table, not a data page, so its bit 31 is not a
             * protection bit. Checking it denied writes to every page beneath
             * a read-only L1 entry - nd500_segment_alloc.c had to force L1
             * entries writable to work around exactly that. */
            if (is_write && l2_pte.protection != 0) {
                MMU_ERR("[MMU] TRAP: PS_ADI write to read-only page! vaddr=0x%08X l1_prot=%d l2_prot=%d\n",
                      virtual_addr, l1_pte.protection, l2_pte.protection);
                /* PVWVIOL: write-protected data page (recoverable - see above). */
                cpu->mmu_pgf_where = MMW_PVWVIOL;
                trap_protect_violation(cpu, cpu->PC, virtual_addr);
                return virtual_addr;  /* Write to read-only page - return virtual address, trap will stop execution */
            }


            physical_pfn = l2_pte.physical_pfn;
            break;
        }

        default:
            /* Invalid index mode */
            MMU_ERR("[MMU] TRAP: Invalid PST index mode %d! vaddr=0x%08X psn=%d\n",
                  pst_entry.index_mode, virtual_addr, psn);
            trap_illegal_operand(cpu, cpu->PC);
            return virtual_addr;  /* Invalid index mode - return virtual address, trap will stop execution */
    }

    /* ─────────────────────────────────────────────────────────
     * Construct physical address: (PFN << 11) | Offset
     * ───────────────────────────────────────────────────────── */

    uint32_t physical_addr = (physical_pfn << PGSHIFT) | offset;

    /* Debug: show translation for high addresses (controlled by show mmu level) */
    if (nd500_dbg_get_mmu_log_level() >= MMU_LOG_ALL && virtual_addr >= 0x08000000) {
        fprintf(stderr, "[MMU] vaddr=0x%08X -> paddr=0x%08X (seg=%d L1=%d L2=%d cap=0x%04X psn=%d mode=%d pfn=0x%X)\n",
                virtual_addr, physical_addr, segment, l1_index, l2_index, capability, psn, pst_entry.index_mode, physical_pfn);
    }

    return physical_addr;
}

/**
 * Trap-free read-only translation for DIAGNOSTICS (harnesses/debugger).
 * Mirrors nd500_mmu_translate_domain's data-read path (is_write=0, is_instruction=0)
 * but NEVER raises a trap or mutates CPU/machine state. Returns the physical address,
 * or 0xFFFFFFFF if the address cannot be translated (unmapped/invalid). Use this to
 * inspect virtual memory without perturbing a running program (the real translate
 * calls trap_page_fault/trap_protect_violation as side effects).
 */
uint32_t nd500_mmu_peek(Nd500Cpu* cpu, uint32_t virtual_addr) {
    if (!cpu) return 0xFFFFFFFFu;
    if (!g_mmu_data_enabled) return virtual_addr; /* MMU off: identity */
    if (!g_pst || !g_pcb_table) return 0xFFFFFFFFu;

    int segment  = (virtual_addr >> SGSHIFT) & 0x1F;
    int l1_index = (virtual_addr >> L1_INDEX_SHIFT) & L1_INDEX_MASK;
    int l2_index = (virtual_addr >> L2_INDEX_SHIFT) & L2_INDEX_MASK;
    int offset   = virtual_addr & (NBPG - 1);
    uint8_t domain = (uint8_t)cpu->CED;

    uint16_t capability = g_pcb_table[domain].data_capabilities[segment];
    if (capability == 0) return 0xFFFFFFFFu;

    int psn = capability & PC_PSN;
    if (psn >= MAX_PST) return 0xFFFFFFFFu;

    PhysicalSegmentTableEntry pst_entry = g_pst[psn];
    uint32_t physical_pfn;

    switch (pst_entry.index_mode) {
        case PS_AZI:
            if (l1_index != 0 || l2_index != 0) return 0xFFFFFFFFu;
            physical_pfn = pst_entry.physical_pfn;
            break;
        case PS_ASI: {
            if (l1_index != 0) return 0xFFFFFFFFu;
            uint32_t pte_addr = (pst_entry.physical_pfn << PGSHIFT) + (l2_index * 4);
            PageTableEntry pte = nd500_mmu_read_pte(cpu, pte_addr);
            if (!pte.valid) return 0xFFFFFFFFu;
            physical_pfn = pte.physical_pfn;
            break;
        }
        case PS_ADI: {
            uint32_t l1_pte_addr = (pst_entry.physical_pfn << PGSHIFT) + (l1_index * 4);
            PageTableEntry l1_pte = nd500_mmu_read_pte(cpu, l1_pte_addr);
            if (!l1_pte.valid) return 0xFFFFFFFFu;
            uint32_t l2_pte_addr = (l1_pte.physical_pfn << PGSHIFT) + (l2_index * 4);
            PageTableEntry l2_pte = nd500_mmu_read_pte(cpu, l2_pte_addr);
            if (!l2_pte.valid) return 0xFFFFFFFFu;
            physical_pfn = l2_pte.physical_pfn;
            break;
        }
        default:
            return 0xFFFFFFFFu;
    }
    return (physical_pfn << PGSHIFT) | offset;
}

/**
 * Default MMU translation using CED (Current Executing Domain).
 * This is a convenience wrapper for code that doesn't need ALT prefix support.
 * For ALT prefix support, use nd500_mmu_translate_domain() with explicit domain.
 */
uint32_t nd500_mmu_translate(Nd500Cpu* cpu, uint32_t virtual_addr, int is_write, int is_instruction) {
    /* Default to CED - most code doesn't use ALT prefix */
    uint8_t domain = cpu ? cpu->CED : 0;
    return nd500_mmu_translate_domain(cpu, virtual_addr, is_write, is_instruction, domain);
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

    /* Mirror into the GUEST Physical Segment Table at PSTP so translate() can
     * read the real table (struct pste: ps_index@[31:30], ps_pfnum@[29:0]). */
    if (cpu && cpu->machine && cpu->PSTP) {
        uint32_t v = ((uint32_t)(index_mode & 0x3) << 30) | (pfn & 0x3FFFFFFF);
        uint32_t a = cpu->PSTP + (uint32_t)psn * 4u;
        nd500_bus_write8(cpu->machine, a,   (uint8_t)(v >> 24));
        nd500_bus_write8(cpu->machine, a+1, (uint8_t)(v >> 16));
        nd500_bus_write8(cpu->machine, a+2, (uint8_t)(v >> 8));
        nd500_bus_write8(cpu->machine, a+3, (uint8_t)v);
    }
}

/* Write a 16-bit capability into the GUEST Domain Information Table at DITBASE.
 * DIT stride 256 bytes/domain; data table at +64, program at +0; segno*2. */
static void mirror_capability_to_dit(Nd500Cpu* cpu, uint8_t domain, int segment,
                                     int is_data, uint16_t capability) {
    if (!cpu || !cpu->machine || !cpu->DITBASE) return;
    uint32_t a = cpu->DITBASE + (uint32_t)domain * 256u + (is_data ? 64u : 0u)
               + (uint32_t)segment * 2u;
    nd500_bus_write8(cpu->machine, a,   (uint8_t)(capability >> 8));
    nd500_bus_write8(cpu->machine, a+1, (uint8_t)capability);
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

/* Active program capability as the TRANSLATE path sees it: the guest DIT in
 * memory when guest-table routing applies to (domain, segment), else the
 * emulator shadow. The indirect CALL/CALLG dispatch must use this - a fork
 * child's new domain exists ONLY in the kernel-written DIT (kpcbinit/newproc
 * write _pcbtab through the seg-28 window onto DITBASE), so reading the
 * mmusetup shadow returned capability 0 for the child's seg-31 syscall gate
 * and its first syscall fell into the SINTRAN MON path (bogus MON LEAVE). */
uint16_t nd500_mmu_get_active_program_capability(Nd500Cpu* cpu, uint8_t domain, int segment) {
    ensure_mmu_tables();
    if (segment < 0 || segment >= MAXSEG) return 0;
    int use_guest = mmu_use_guest_tables() && cpu && cpu->machine && cpu->DITBASE
                 && (domain != 0
                     || segment == 2 || segment == 3 || segment == 4 || segment == 5
                     || segment == 7 || segment == 26 || segment == 29
                     || segment == 30 || segment == 31);
    if (use_guest) {
        uint32_t cap_addr = cpu->DITBASE + (uint32_t)domain * 256u + (uint32_t)segment * 2u;
        return (uint16_t)(((uint32_t)nd500_bus_read8(cpu->machine, cap_addr) << 8)
                        |  (uint32_t)nd500_bus_read8(cpu->machine, cap_addr + 1));
    }
    return g_pcb_table ? g_pcb_table[domain].program_capabilities[segment] : 0;
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
    mirror_capability_to_dit(cpu, domain, segment, 0 /*program*/, capability);
}

void nd500_mmu_set_data_capability(Nd500Cpu* cpu, uint8_t domain, int segment, uint16_t capability) {
    if (!g_pcb_table || segment < 0 || segment >= MAXSEG) {
        return;
    }
    /* Note: domain is uint8_t (0-255), MAXDOM is 256, range check not needed */
    g_pcb_table[domain].data_capabilities[segment] = capability;
    mirror_capability_to_dit(cpu, domain, segment, 1 /*data*/, capability);
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

    /* PTE format (ND-500 hardware, struct pte in machine/pte.h, big-endian):
     * [31]   = pg_prot  write protection (PR_RO=0x80000000; 1=read-only)
     * [30]   = reserved
     * [29:0] = pg_pfnum physical page frame number (PR_PPN=0x3FFFFFFF)
     * A zero entry means "no mapping" -> valid = (pfnum != 0).
     */
    pte.protection = (uint8_t)((pte_value >> 31) & 0x1);
    pte.physical_pfn = pte_value & 0x3FFFFFFF;
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

    /* ND-500 hardware format (machine/pte.h): pg_prot@31, pg_pfnum@[29:0]. */
    uint32_t pte_value = ((uint32_t)(pte.protection & 1) << 31) |
                         (pte.physical_pfn & 0x3FFFFFFF);

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
        if (!nd500_quiet) printf("ND-500: DCTSB - Data cache TSB cleared\n");
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
        if (!nd500_quiet) printf("ND-500: PCTSB - Program cache TSB cleared\n");
    }
}

// ═══════════════════════════════════════════════════════
// MMU TABLE STATE SNAPSHOT (for nested UECOM runs)
// ═══════════════════════════════════════════════════════

/* Snapshot/restore of the C-side MMU tables (g_pst + g_pcb_table), mirroring
 * nd500_segment_alloc_state_save/_restore. A nested 317B UECOM DOM load
 * overwrites PST entries and capabilities the CALLER's domain still
 * references; the caller's RAM/CPU snapshot alone does not cover these
 * tables, so without this the caller resumes with a wrong virtual-to-
 * physical translation (heap vars, THA vector, stack limits all read from
 * another domain's pages). See
 * docs/HANDOFF-NC-HEAP-CRASH-2026-07-27.md section 2c. */

typedef struct {
    PhysicalSegmentTableEntry pst[MAX_PST];
    ProcessControlBlock       pcb[MAXDOM];
} MmuStateBlob;

void* nd500_mmu_state_save(void) {
    ensure_mmu_tables();
    if (!g_pst || !g_pcb_table) return NULL;
    MmuStateBlob* b = (MmuStateBlob*)malloc(sizeof(MmuStateBlob));
    if (!b) return NULL;
    memcpy(b->pst, g_pst, MAX_PST * sizeof(PhysicalSegmentTableEntry));
    memcpy(b->pcb, g_pcb_table, MAXDOM * sizeof(ProcessControlBlock));
    return b;
}

void nd500_mmu_state_restore(void* blob) {
    if (!blob) return;
    MmuStateBlob* b = (MmuStateBlob*)blob;
    if (g_pst)
        memcpy(g_pst, b->pst, MAX_PST * sizeof(PhysicalSegmentTableEntry));
    if (g_pcb_table)
        memcpy(g_pcb_table, b->pcb, MAXDOM * sizeof(ProcessControlBlock));
    free(blob);
}
