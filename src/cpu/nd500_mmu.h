#pragma once
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
 * Level 1: Virtual Address → Capability (via PCB[domain].pcb_dc[segment] or pcb_pc[segment])
 * Level 2: Capability → PST Entry (via PST[PSN])
 * Level 3a: Direct (PS_AZI) → Physical Page
 * Level 3b: Single-level (PS_ASI) → PTE → Physical Page
 * Level 3c: Two-level (PS_ADI) → L1 PTE → L2 PTE → Physical Page
 *
 * Reference: C# CpuND500.MMU.cs from RetroCore emulator
 */

// ═══════════════════════════════════════════════════════
// MMU CONSTANTS
// ═══════════════════════════════════════════════════════

#define NBPG            2048        /* Bytes per page (2KB) */
#define PGSHIFT         11          /* LOG2(NBPG) */
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
#define DC_SHSEG        6           /* Shared segment (ND-100 ↔ ND-500) */
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

// ═══════════════════════════════════════════════════════
// MMU DATA STRUCTURES
// ═══════════════════════════════════════════════════════

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

// ═══════════════════════════════════════════════════════
// MMU FUNCTION DECLARATIONS
// ═══════════════════════════════════════════════════════

/* MMU Initialization */
void nd500_mmu_init(Nd500Cpu* cpu);

/* Data MMU Control (DMON/DMOF instructions) */
void nd500_mmu_enable_data(Nd500Cpu* cpu);
void nd500_mmu_disable_data(Nd500Cpu* cpu);
int nd500_mmu_is_data_enabled(Nd500Cpu* cpu);

/* Program MMU Control (PMON/PMOF instructions) */
void nd500_mmu_enable_program(Nd500Cpu* cpu);
void nd500_mmu_disable_program(Nd500Cpu* cpu);
int nd500_mmu_is_program_enabled(Nd500Cpu* cpu);

/* Legacy MMU Control (enables/disables BOTH data and program MMU) */
void nd500_mmu_enable(Nd500Cpu* cpu);
void nd500_mmu_disable(Nd500Cpu* cpu);
int nd500_mmu_is_enabled(Nd500Cpu* cpu);

/* MMU Address Translation */
uint32_t nd500_mmu_translate(Nd500Cpu* cpu, uint32_t virtual_addr, int is_write, int is_instruction);
uint32_t nd500_mmu_phyladr(Nd500Cpu* cpu, uint32_t virtual_addr);

/* PST Accessors */
PhysicalSegmentTableEntry nd500_mmu_get_pst_entry(Nd500Cpu* cpu, int psn);
void nd500_mmu_set_pst_entry(Nd500Cpu* cpu, int psn, uint8_t index_mode, uint32_t pfn);

/* PCB Accessors */
ProcessControlBlock* nd500_mmu_get_pcb(Nd500Cpu* cpu, uint8_t domain);
uint16_t nd500_mmu_get_program_capability(Nd500Cpu* cpu, uint8_t domain, int segment);
uint16_t nd500_mmu_get_data_capability(Nd500Cpu* cpu, uint8_t domain, int segment);
void nd500_mmu_set_program_capability(Nd500Cpu* cpu, uint8_t domain, int segment, uint16_t capability);
void nd500_mmu_set_data_capability(Nd500Cpu* cpu, uint8_t domain, int segment, uint16_t capability);

/* PTE Read/Write (from physical memory) */
PageTableEntry nd500_mmu_read_pte(Nd500Cpu* cpu, uint32_t physical_addr);
void nd500_mmu_write_pte(Nd500Cpu* cpu, uint32_t physical_addr, PageTableEntry pte);

/* Cache Control */
void nd500_mmu_clear_data_cache_tsb(Nd500Cpu* cpu);
void nd500_mmu_clear_program_cache_tsb(Nd500Cpu* cpu);
