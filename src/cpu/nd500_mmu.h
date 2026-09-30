/*
 * nd500_mmu.h - MMU three-level address translation, declarations
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#ifndef ND500_MMU_H
#define ND500_MMU_H
#include <stdint.h>

/*
 * ND-500 Memory Management Unit (MMU) - Three-Level Address Translation
 *
 * Architecture:
 * - Segmented addressing: 32-bit virtual addresses (5-bit segment, 7-bit L1, 9-bit L2, 11-bit offset)
 * - Process Control Blocks (PCB): Per-domain capability tables (256 domains)
 * - Physical Segment Table (PST): Maps segments to physical memory (8192 entries)
 * - Two-level page tables: First and second level indirection
 * - Separate I&D spaces: Independent instruction and data address spaces
 *
 * Translation Flow:
 * Level 1: Virtual Address -> Capability (via PCB[domain].pcb_dc[segment] or pcb_pc[segment])
 * Level 2: Capability -> PST Entry (via PST[PSN])
 * Level 3a: Direct (PS_AZI) -> Physical Page
 * Level 3b: Single-level (PS_ASI) -> PTE -> Physical Page
 * Level 3c: Two-level (PS_ADI) -> L1 PTE -> L2 PTE -> Physical Page
 *
 * Reference: C# CpuND500.MMU.cs from RetroCore emulator
 */

// =======================================================
// MMU CONSTANTS
// =======================================================

#define NBPG            2048        /* Bytes per page (2KB) */
#define PGSHIFT         11          /* LOG2(NBPG) */
#define PGOFSET         (NBPG - 1)  /* byte offset within a page */
#define NBSG            0x8000000   /* Bytes per segment (128MB) */
#define SGSHIFT         27          /* LOG2(NBSG) */

/* ND-500 Address Decomposition (per ND-05.009.4 Reference Manual, p53-54)
 * Virtual address format: [Segment(5) | L1 Index(7) | L2 Index(9) | Offset(11)]
 */
#define L1_INDEX_SHIFT  20          /* Shift to get L1 index (bits 26-20) */
#define L1_INDEX_MASK   0x7F        /* 7 bits for L1 (128 entries max) */
#define L2_INDEX_SHIFT  11          /* Shift to get L2 index (bits 19-11) */
#define L2_INDEX_MASK   0x1FF       /* 9 bits for L2 (512 entries max) */
#define NPTEPG          512         /* Page table entries per page (2048/4) */
#define MAXSEG          32          /* Segments per domain */
#define MAXDOM          256         /* Domains per process */
#define PCBSIZ          256         /* Each PCB is 256 bytes */
#define MAX_PST         8192        /* Maximum PST entries */

/* PST Indexing Modes */
#define PS_AZI          0           /* Direct addressed page (no indirection) */
#define PS_ASI          1           /* Single index page (one level) */
#define PS_ADI          2           /* Double index page (two levels) */

/* Capability Masks (Program Capability) */
#define PC_TYP          0x8000      /* Capability Type bit */
#define PC_DIR          0x0000      /* Direct Segment */
#define PC_IND          0x8000      /* Indirect Segment */
#define PC_PSN          0x1FFF      /* Physical Segment Number mask (13 bits) */
#define PC_OMC          0x4000      /* Other Machine */
#define PC_DOM          0x1FE0      /* Domain Number mask */
#define PC_SEG          0x001F      /* Segment Number mask */

/* MMU status "where the fault occurred" codes (MMWHERE nibble of the fault
 * information word, plus MMINST for an I-channel access). Ground truth is the
 * NDIX kernel: machine/icb.h defines MMWHERE/MMINST/PVWVIOL/PFZPST/PFZ1/PFZ2,
 * and machine/trap.c:decodetrap()'s mmtraptype[] names every value. The kernel
 * reads this out of the trap frame as cx_info and branches on it - T_PV only
 * attempts pagein() when (info&MMWHERE)==PVWVIOL and MMINST is clear, and
 * T_PGF only services PFZ2 - so the code the MMU records here decides whether
 * a fault is recovered or panics. */
#define MMW_MASK        0xF         /* MMWHERE nibble */
#define MMW_ALTVIOL     0x1         /* alt protect violation */
#define MMW_PVWVIOL     0x2         /* write protect violation */
#define MMW_INDEXERR    0x3         /* index error (PSN out of range) */
#define MMW_IND_OTHER   0x6         /* indirect capability to another machine */
#define MMW_IND_SAME    0x7         /* indirect capability within the machine */
#define MMW_ZEROCAP     0x8         /* zero in the capability */
#define MMW_PFZPST      0xD         /* 0 in PST entry */
#define MMW_PFZ1        0xE         /* 0 in second level index entry */
#define MMW_PFZ2        0xF         /* 0 in last level index entry */
#define MMW_INST        0x40        /* MMINST: fault on an I-channel access */

/* Capability Masks (Data Capability) */
#define DC_WRP          0x8000      /* Write Permitted */
#define DC_PAC          0x4000      /* Parameter Access (user mode) */
#define DC_SHS          0x2000      /* Shared Segment (disable cache) */
#define DC_PSN          0x1FFF      /* Physical Segment Number mask */

/* Permission Constants */
#define SG_RW           DC_WRP                  /* Kernel read/write */
#define SG_RO           0                       /* Kernel read-only */
#define SG_URW          (DC_WRP | DC_PAC)       /* User read/write */
#define SG_URO          DC_PAC                  /* User read-only */

/* Page Table Entry Protection */
#define PG_W            0           /* Writable */
#define PG_R            1           /* Read-only */

/* Well-Known PCB Data Capability Indices */
#define DC_KDATA        0           /* Kernel data */
#define DC_KTEXT        1           /* Kernel text */
#define DC_PHYS         2           /* Physical memory array */
#define DC_SYS          3           /* System tables */
#define DC_UPT          4           /* User page tables */
#define DC_SPT          5           /* Shadow user page tables */
#define DC_SHSEG        6           /* Shared segment (ND-100 <-> ND-500) */
#define DC_NCSYS        7           /* System tables (no-cache) */
#define DC_CXBTAB       8           /* Context block table */
#define DC_UTEXT        26          /* User text */
#define DC_PST          27          /* PST itself */
#define DC_PS           28          /* Process segment (PCB table) */
#define DC_KSTACK       29          /* Kernel stack */
#define DC_UDATA        30          /* User data */
#define DC_USTACK       31          /* User stack */

/* Well-Known PCB Program Capability Indices */
#define PC_KTEXT_IDX    0           /* Kernel text (renamed to avoid conflict) */
#define PC_OM           31          /* Other machine (ND-100) */

/* PST Index Constants */
#define ADDRPST         0           /* Debugger assist */
#define TEXTINDEX       1           /* Kernel text */
#define DATAINDEX       2           /* Kernel data */
#define STACKINDEX      3           /* Kernel stack */
#define PSTINDEX        4           /* PST itself */
#define SYSINDEX        5           /* System tables */
#define PHYSINDEX       6           /* Physical memory array */
#define PSINDEX         7           /* Process segment (PCB table) */
#define USRPTINDEX      8           /* User page tables */
#define SUSRPTINDEX     9           /* Shadow user page tables */
#define SHAREINDEX      10          /* Shared segment */
#define NCSYSINDEX      11          /* Non-cached system tables */
#define CXBTABINDEX     12          /* Context block table */
#define FIRST_PHYS_SEG  13          /* First PST entry used by NDIX */

// =======================================================
// MMU DATA STRUCTURES
// =======================================================

/**
 * Physical Segment Table Entry (PSTE) - 4 bytes
 * Maps virtual segments to physical pages or page tables
 */
typedef struct {
    uint8_t index_mode;         /* PS_AZI, PS_ASI, or PS_ADI */
    uint32_t physical_pfn;      /* Physical page frame number (30 bits) */
} PhysicalSegmentTableEntry;

/**
 * Page Table Entry (PTE) - 4 bytes
 * Used in single-level and two-level page tables
 * Format: [31:2]=PFN, [1]=valid/present, [0]=protection
 */
typedef struct {
    uint8_t valid;              /* 0=not present (page fault), 1=present */
    uint8_t protection;         /* PG_W (0) or PG_R (1) */
    uint32_t physical_pfn;      /* Physical page frame number (30 bits) */
} PageTableEntry;

/**
 * Process Control Block (PCB) - 256 bytes
 * Contains capabilities for each segment in a domain
 * This is a subset of the full PCB structure
 */
typedef struct {
    uint16_t program_capabilities[MAXSEG];      /* pcb_pc[32] - Program capabilities */
    uint16_t data_capabilities[MAXSEG];         /* pcb_dc[32] - Data capabilities */

    /* Domain call information (at offset 128 in real PCB) */
    uint8_t calling_domain;                     /* call_ce */
    uint8_t alternative_domain;                 /* call_ca */
    uint32_t calling_p;                         /* call_p - P of calling domain */
    uint32_t calling_b;                         /* call_b - B of calling domain */

    /* Trap handling information */
    uint8_t trapped_domain;                     /* trap_ce */
    uint8_t trap_alternative_domain;            /* trap_ca */
    uint64_t status_register;                   /* trap_st1, trap_st2 */

    /* Trap enable masks (64-bit each) */
    uint64_t own_trap_enable;                   /* pcb_ote1, pcb_ote2 */
    uint64_t child_trap_enable;                 /* pcb_cte1, pcb_cte2 */
    uint64_t mother_trap_enable;                /* pcb_mte1, pcb_mte2 */
    uint64_t trap_enable_mod_mask;              /* pcb_temm1, pcb_temm2 */

    /* Domain state registers (saved during domain switch) */
    uint32_t trap_handler_address;              /* pcb_tha */
    uint8_t mother_domain;                      /* pcb_md */
    uint8_t inside_trap_handler;                /* pcb_ith (boolean) */
    uint32_t top_of_stack;                      /* pcb_tos */
    uint32_t lower_limit;                       /* pcb_ll */
    uint32_t higher_limit;                      /* pcb_hl */
    uint8_t privileged_instructions_allowed;    /* pcb_pia (boolean) */

    uint8_t current_alternative_domain;         /* pcb_cad */
    uint8_t current_executing_domain;           /* pcb_ced */
} ProcessControlBlock;

// Forward declaration of CPU type
typedef struct Nd500Cpu Nd500Cpu;

// =======================================================
// MMU FUNCTION DECLARATIONS
// =======================================================

/* MMU Initialization */
void nd500_mmu_init(Nd500Cpu* cpu);

/* Data MMU Control (DMON/DMOF instructions) */
void nd500_mmu_enable_data(Nd500Cpu* cpu);
void nd500_mmu_disable_data(Nd500Cpu* cpu);
int nd500_mmu_is_data_enabled(Nd500Cpu* cpu);

/* Translate an address on a PHYSICAL SEGMENT (RPHS/WPHS, ND-05.009.4
 * 16.31/16.32). The segment number is GIVEN (from I4), so no capability is
 * consulted and the walk is entered at the PST. Returns the physical address,
 * or the input unchanged when the data MMU is off. Raises the normal traps on
 * a bad PSN or a failed walk - check nd500_trap_occurred() / instr_aborted. */
uint32_t nd500_mmu_translate_physical_segment(Nd500Cpu* cpu, uint32_t psn,
                                              uint32_t segment_relative_addr,
                                              int is_write);

/* Program MMU Control (PMON/PMOF instructions) */
void nd500_mmu_enable_program(Nd500Cpu* cpu);
void nd500_mmu_disable_program(Nd500Cpu* cpu);
int nd500_mmu_is_program_enabled(Nd500Cpu* cpu);

/* Legacy MMU Control (enables/disables BOTH data and program MMU) */
/**
 * @brief Install a per-regime guest-table routing policy, or remove one.
 *
 * @param cpu    The CPU. NULL is ignored.
 * @param policy Called for each translation with the domain and segment; returns
 *               non-zero to walk the guest's DIT/PST instead of the emulator
 *               shadow tables. The regime's own enable flag belongs INSIDE it, so
 *               that a regime whose flag is off behaves exactly as it did before
 *               any policy existed. NULL restores the architectural default: walk
 *               the guest tables whenever DITBASE and PSTP are both set.
 * @param ctx    Opaque context handed back to the policy.
 * @return Nothing.
 */
/**
 * @brief Declare the guest's Domain Information Table base, without touching memory.
 *
 * Records the base and marks the DIT configured. ZERO IS A LEGITIMATE BASE and is
 * accepted as one - see Nd500Cpu::dit_configured for what assuming otherwise cost.
 *
 * DECLARING IS NOT SETTING UP. This function writes NOTHING to guest memory. The
 * ND-500's DIT entries are whole 256-byte process control blocks, and the guest
 * fills the trap-control fields itself before the CPU is ever started - on the
 * octobus that is a run of PHYSWR transfers. A routine that zeroed those entries
 * while declaring the base would erase exactly what the guest just wrote, and the
 * resulting zero trap-handler address reads as though the declaration had done
 * nothing at all. Ported from RetroCore CpuND500.Domain.cs DeclareDitBase, whose
 * SetupDIT twin does the zeroing and must not be used here.
 *
 * @param cpu  The CPU. NULL is ignored.
 * @param base Byte address of the table, possibly 0.
 * @return Nothing.
 */
void nd500_mmu_declare_dit_base(Nd500Cpu* cpu, uint32_t base);

/**
 * @brief Find a process's capability table through PS and declare it as the DIT base.
 *
 * THE WALK STARTS AT PS, NOT AT A BASE SOMEONE LEARNED BY WATCHING WRITES. The
 * capability of a logical segment lies at
 *
 *     process_segment_base + CED*256 + (is_instruction ? 0 : 64) + segment*2
 *
 * and the process segment is reached from PS, which is an INDEX into the physical
 * segment table rather than an address - ND-05.020.01 section 6.6: "This register
 * points to an element of the Physical Segment Table. The PST element addresses the
 * process segment of the process." Chapter 11 walks it in two nanostates, PSCAPA
 * then PSCAPT, and PSCAPT refuses double indexing outright: "The indexing for this
 * physical segment has two levels. This is not allowed for a process segment."
 * DIRECT and SINGLE therefore resolve here and DOUBLE is an error return.
 *
 * MEASURED, and this is why the function exists: on the ND-5000 octobus lane the
 * ND-500/5000 monitor's own SINTRAN writes trap configuration into one 256-byte
 * block before starting a process, and a base learned from those writes pointed at
 * a table whose segment-1 capability named physical segment 83 - whose PST entry was
 * zero, so the very first instruction fetch page-faulted. PS is the guest's own
 * answer to the same question.
 *
 * @param cpu      The CPU, whose PSTP must already be set. NULL fails.
 * @param ps       The process-segment index, 1..MAX_PST-1. Zero fails: PS zero is
 *                 "no process segment", not entry zero.
 * @param out_base Receives the byte address of the capability table. May be NULL.
 * @return 0 once the base is declared, -1 when PS is out of range, the PST entry is
 *         zero, its index page is not valid, or it asks for double indexing.
 */
int nd500_mmu_declare_process_segment(Nd500Cpu* cpu, uint32_t ps, uint32_t* out_base);

void nd500_mmu_set_guest_table_policy(Nd500Cpu* cpu,
                                      int (*policy)(void *ctx, uint8_t domain, int segment),
                                      void *ctx);

void nd500_mmu_enable(Nd500Cpu* cpu);
void nd500_mmu_disable(Nd500Cpu* cpu);
int nd500_mmu_is_enabled(Nd500Cpu* cpu);

/* MMU Address Translation */
uint32_t nd500_mmu_translate(Nd500Cpu* cpu, uint32_t virtual_addr, int is_write, int is_instruction);
uint32_t nd500_mmu_translate_domain(Nd500Cpu* cpu, uint32_t virtual_addr, int is_write, int is_instruction, uint8_t domain);

/* Demand-growth for MON-connected segments (412B FSCNT / 422B GSWSP).
 * Called from the PS_ADI translation path when a page is missing: allocates the
 * absent L2 table and/or data page so the access can be retried. Returns 1 if
 * the fault was resolved, 0 to trap as usual. Defined in nd500_segment_alloc.c. */
int nd500_segment_grow_on_fault(void* cpu_ptr, uint32_t virtual_addr, uint8_t domain);

/* Build a DOM DATA segment as a PS_ADI (two-level) GROWABLE segment that ADOPTS
 * the loader's already-loaded, physically contiguous initialized pages (PFNs
 * data_phys_base.. for data_pages pages) and registers it demand-growable.
 * Replaces the old PS_ASI DATA setup, which capped a DATA segment at 1 MB (L1
 * must be 0) and crashed large programs (e.g. the NC/CAT-500 C code generator)
 * that write past 1 MB into their DSEG.
 * New pages (L1/L2 tables, grown data) come from the machine's physical page
 * allocator, which owns every frame - there is no floor to pass and no way to
 * collide with the DOM image. Returns 0 on success, -1 on failure.
 * Defined in nd500_segment_alloc.c. */
int nd500_segment_adopt_growable_data(void* cpu_ptr, void* machine_ptr,
    uint8_t domain, uint32_t segment, int psn,
    uint32_t data_phys_base, uint32_t data_pages);

/* Build a DOM DATA segment as BOUNDED PS_ADI two-level: adopt the loaded
 * initialized pages (data_pages from data_phys_base) + eagerly map
 * reserve_pages of fresh zeroed pages, and do NOT register it growable, so an
 * access past the owned extent TRAPS to the guest THA exactly like the old
 * PS_ASI path (the fault boundary NC's codegen relies on) while the owned
 * extent may exceed the PS_ASI 1 MB cap (FILE-COMPARE ships a 2 MB DSEG).
 * Returns the L1 table PFN (>0) so the caller can share the tables via a
 * plain capability alias, or 0 on failure. Defined in nd500_segment_alloc.c.
 * Design: PLAN-nd500x-growable-DATA-option2-redesign-2026-07-26.md option b. */
uint32_t nd500_segment_map_bounded_data(void* cpu_ptr, void* machine_ptr,
    int psn, uint32_t data_phys_base, uint32_t data_pages,
    uint32_t reserve_pages);

/* Register a growable ALIAS so demand-growth for alias_segment reuses the same
 * two-level tables as source_segment (the DOM loader's FORTRAN/compiler
 * segment-0 -> segment-1 DATA alias). Defined in nd500_segment_alloc.c. */
int nd500_segment_register_growable_alias(uint8_t domain, uint32_t alias_segment,
                                          uint32_t source_segment);

/* Trap-free read-only translate for diagnostics; 0xFFFFFFFF if unmapped. Never perturbs state. */
uint32_t nd500_mmu_peek(Nd500Cpu* cpu, uint32_t virtual_addr);

/* Same, but for a NAMED domain instead of whichever one is currently running.
 * Anything driven by an interrupt, a poll or a timer must use this: the CPU
 * could be anywhere when it fires, and the data it wants belongs to a
 * particular domain. See the comment on the definition for the measured case
 * (the XMSG rings in the kernel's segment 6, reached from the clock tick). */
uint32_t nd500_mmu_peek_domain(Nd500Cpu* cpu, uint32_t virtual_addr,
                               uint8_t domain);
uint32_t nd500_mmu_phyladr(Nd500Cpu* cpu, uint32_t virtual_addr);

/* PST Accessors */
PhysicalSegmentTableEntry nd500_mmu_get_pst_entry(Nd500Cpu* cpu, int psn);
void nd500_mmu_set_pst_entry(Nd500Cpu* cpu, int psn, uint8_t index_mode, uint32_t pfn);

/* PCB Accessors */
ProcessControlBlock* nd500_mmu_get_pcb(Nd500Cpu* cpu, uint8_t domain);
uint16_t nd500_mmu_get_program_capability(Nd500Cpu* cpu, uint8_t domain, int segment);
/* Capability as the translate path resolves it (guest DIT memory when guest-
 * table routing applies, else the shadow) - use for CALL/CALLG dispatch. */
uint16_t nd500_mmu_get_active_program_capability(Nd500Cpu* cpu, uint8_t domain, int segment);
uint16_t nd500_mmu_get_data_capability(Nd500Cpu* cpu, uint8_t domain, int segment);
void nd500_mmu_set_program_capability(Nd500Cpu* cpu, uint8_t domain, int segment, uint16_t capability);
void nd500_mmu_set_data_capability(Nd500Cpu* cpu, uint8_t domain, int segment, uint16_t capability);

/* PTE Read/Write (from physical memory) */
PageTableEntry nd500_mmu_read_pte(Nd500Cpu* cpu, uint32_t physical_addr);
void nd500_mmu_write_pte(Nd500Cpu* cpu, uint32_t physical_addr, PageTableEntry pte);

/* Cache Control */
void nd500_mmu_clear_data_cache_tsb(Nd500Cpu* cpu);
void nd500_mmu_clear_program_cache_tsb(Nd500Cpu* cpu);

/* Snapshot/restore the C-side MMU tables (PST + PCB capabilities) around a
 * nested 317B UECOM run - the nested DOM load overwrites entries the caller's
 * domain still references. Pairs with nd500_segment_alloc_state_save/_restore. */
/*
 * PER-CPU MMU STATE.
 *
 * The PST, the PCB table and the two I&D enable flags are one CPU's state, not
 * the process's. With several ND-5000s on the MFbus, sharing them would mean CPU
 * 1's DMON switching on CPU 2's data MMU and CPU 1's capability edits being
 * visible in CPU 2's address space - a wrong-answer bug, not a performance one.
 *
 * The struct is allocated by nd500_cpu_init() and released by nd500_cpu_free().
 * The two TABLES inside it stay lazily allocated, as they always were: they are
 * 8192 and 256 entries and a CPU that never enables the MMU never needs them.
 */
typedef struct Nd500MmuState {
    PhysicalSegmentTableEntry* pst;         /* MAX_PST entries, or NULL until used */
    ProcessControlBlock*       pcb_table;   /* MAXDOM entries, or NULL until used */
    int                        data_enabled;     /* DMON / DMOF */
    int                        program_enabled;  /* PMON / PMOF */
} Nd500MmuState;

/* Allocate / release the per-CPU MMU state. Safe on NULL and idempotent. */
Nd500MmuState* nd500_mmu_state_create(void);
void           nd500_mmu_state_free(Nd500MmuState* state);

void* nd500_mmu_state_save(Nd500Cpu* cpu);
void  nd500_mmu_state_restore(Nd500Cpu* cpu, void* blob);

/* Open / close a per-run allocation scope (nd500_segment_alloc.c). Save before
 * a DOM is loaded, restore when it exits: the restore frees every physical page
 * the run allocated and puts the growable-segment registry back, so a nested
 * run cannot leak into - or over - its caller. */
/**
 * @brief MON callback for 422B GSWSP: allocate an empty, writable scratch
 *        segment backed by MMU/PST pages.
 *
 * @param cpu_ptr               The calling Nd500Cpu.
 * @param machine_ptr           The Nd500Machine.
 * @param domain                Domain number; 0xFF means the current domain.
 * @param requested_segment     Logical segment wanted.
 * @param segment_size_bytes    Size of the segment.
 * @param out_assigned_segment  Receives the segment actually assigned.
 * @return 0 on success, a SINTRAN error code otherwise.
 */
int nd500_mon_allocate_segment(void *cpu_ptr, void *machine_ptr, uint8_t domain,
                               uint32_t requested_segment, uint32_t segment_size_bytes,
                               uint32_t *out_assigned_segment);

/**
 * @brief MON callback for 412B FSCNT: connect an open file as a data segment.
 *
 * Allocates an MMU/PST-backed segment the same way as GSWSP, then copies the
 * file's bytes into it verbatim for access types 0, 2 and 3 (type 1 leaves
 * it zeroed).
 *
 * @param cpu_ptr               The calling Nd500Cpu.
 * @param machine_ptr           The Nd500Machine.
 * @param domain                Domain number; 0xFF means the current domain.
 * @param requested_segment     Logical segment wanted.
 * @param access_type           0 initial data, 1 uninitialized, 2 primarily
 *                              sequential, 3 combination of 1 and 2.
 * @param writable              Non-zero for an RW capability, from the file's
 *                              open mode (not from access_type).
 * @param host_path             Host file to load.
 * @param file_size_bytes       Size of that file.
 * @param out_assigned_segment  Receives the segment actually assigned.
 * @return 0 on success, a SINTRAN error code otherwise.
 */
int nd500_mon_connect_file_as_segment(void *cpu_ptr, void *machine_ptr, uint8_t domain,
                                      uint32_t requested_segment, uint32_t access_type,
                                      int writable, const char *host_path,
                                      uint32_t file_size_bytes,
                                      uint32_t *out_assigned_segment);

/**
 * @brief Write a file-connected segment's mapped pages back to its host file.
 *
 * Only mapped pages are written; untouched pages of a demand-grown segment
 * stay holes, and the file is extended to cover the highest mapped page.
 *
 * @param cpu_ptr    The calling Nd500Cpu.
 * @param domain     Real domain number (callers resolve 0xFF first).
 * @param segment    Logical segment.
 * @param host_path  Host file to write.
 * @return 1 if the segment was flushed, 0 if there was nothing to flush (not
 *         a tracked segment, or mapped read-only), negative on a write error.
 */
int nd500_segment_writeback(void *cpu_ptr, uint8_t domain, uint32_t segment,
                            const char *host_path);

/**
 * @brief Release a file-connected segment: clear its registry slot, PST entry
 *        and the domain's data capability, so the same logical segment can be
 *        connected again.
 *
 * @param cpu_ptr  The calling Nd500Cpu.
 * @param domain   Domain number; 0xFF means the current executing domain.
 * @param segment  Logical segment.
 */
void nd500_segment_release(void *cpu_ptr, uint8_t domain, uint32_t segment);

void* nd500_segment_alloc_state_save(void* machine_ptr);
void  nd500_segment_alloc_state_restore(void* blob);

#endif /* ND500_MMU_H */
