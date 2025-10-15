#pragma once
#include <stdint.h>

/*
 * ND-500 Domain System - Cross-Domain Calling and Protection
 *
 * The domain system provides process isolation and controlled cross-domain
 * communication on the ND-500. Each domain has its own:
 * - Address space (via separate PCB capability tables)
 * - Stack limits (TOS, LL, HL)
 * - Trap handler (THA)
 * - Privilege level (PIA flag)
 *
 * Key Components:
 * - CED (Current Executing Domain): The active domain number
 * - CAD (Current Alternative Domain): Domain for data access
 * - DIT (Domain Information Table): Per-domain state storage
 * - PCB (Process Control Block): Per-domain capabilities and call state
 *
 * Cross-Domain Calls:
 * 1. Caller invokes code in different domain
 * 2. System saves caller context to target PCB.pcb_call
 * 3. Domain registers (CAD, CED) updated
 * 4. Target domain state loaded from DIT
 * 5. Execution continues in target domain
 * 6. Target can return to caller via domain return
 *
 * Reference: C# CpuND500.Domain.cs from RetroCore emulator
 */

// ═══════════════════════════════════════════════════════
// DOMAIN CONSTANTS
// ═══════════════════════════════════════════════════════

#define KERNEL_DOMAIN       0           /* Domain 0 is always kernel */
#define MAX_DOMAINS         256         /* Maximum domains per process */

/* DIT (Domain Information Table) Structure */
#define DIT_ENTRY_SIZE      16          /* 16 bytes per domain entry */
#define DIT_TOS_OFFSET      0           /* Top of Stack offset */
#define DIT_LL_OFFSET       4           /* Lower Limit offset */
#define DIT_HL_OFFSET       8           /* Higher Limit offset */
#define DIT_THA_OFFSET      12          /* Trap Handler Address offset */

/* PCB Call State Structure (at PCB offset 128) */
#define PCB_SIZE            256         /* Total PCB size */
#define PCB_CALL_OFFSET     128         /* Offset to call state structure */
#define PCB_CALL_CE_OFFSET  0           /* Calling executing domain (1 byte) */
#define PCB_CALL_CA_OFFSET  1           /* Calling alternative domain (1 byte) */
#define PCB_CALL_P_OFFSET   4           /* Calling P register (4 bytes) */
#define PCB_CALL_B_OFFSET   8           /* Calling B register (4 bytes) */

/* Domain Boundary Markers */
#define PREVB_MARKER        0           /* PREVB=0 marks domain boundary */
#define RETA_MARKER         0           /* RETA=0 marks domain boundary */

// ═══════════════════════════════════════════════════════
// DOMAIN STRUCTURES
// ═══════════════════════════════════════════════════════

/**
 * Domain Call State
 * Saved in target PCB when cross-domain call occurs
 */
typedef struct {
    uint8_t calling_domain;         /* CED of calling domain */
    uint8_t alternative_domain;     /* CAD of calling domain */
    uint32_t calling_p;             /* P register of caller */
    uint32_t calling_b;             /* B register of caller */
} DomainCallState;

/**
 * Domain Information Table Entry
 * Stored in memory at DITBASE + (domain * 16)
 */
typedef struct {
    uint32_t TOS;                   /* Top of Stack */
    uint32_t LL;                    /* Lower Limit */
    uint32_t HL;                    /* Higher Limit */
    uint32_t THA;                   /* Trap Handler Address */
} DomainInfoEntry;

// Forward declaration of CPU type
typedef struct Nd500Cpu Nd500Cpu;

// ═══════════════════════════════════════════════════════
// DOMAIN FUNCTION DECLARATIONS
// ═══════════════════════════════════════════════════════

/* Domain System Initialization */
void nd500_domain_init(Nd500Cpu* cpu);
void nd500_domain_setup_dit(Nd500Cpu* cpu, uint32_t ditbase);

/* Domain Switching */
void nd500_domain_switch(Nd500Cpu* cpu, uint8_t target_domain, uint32_t entry_point);
void nd500_domain_return(Nd500Cpu* cpu);
int nd500_domain_is_boundary(Nd500Cpu* cpu);

/* Domain State Management */
void nd500_domain_save_state(Nd500Cpu* cpu, uint8_t domain);
void nd500_domain_load_state(Nd500Cpu* cpu, uint8_t domain);

/* DIT Access Functions */
uint32_t nd500_domain_read_tos(Nd500Cpu* cpu, uint8_t domain);
uint32_t nd500_domain_read_ll(Nd500Cpu* cpu, uint8_t domain);
uint32_t nd500_domain_read_hl(Nd500Cpu* cpu, uint8_t domain);
uint32_t nd500_domain_read_tha(Nd500Cpu* cpu, uint8_t domain);

void nd500_domain_write_tos(Nd500Cpu* cpu, uint8_t domain, uint32_t value);
void nd500_domain_write_ll(Nd500Cpu* cpu, uint8_t domain, uint32_t value);
void nd500_domain_write_hl(Nd500Cpu* cpu, uint8_t domain, uint32_t value);
void nd500_domain_write_tha(Nd500Cpu* cpu, uint8_t domain, uint32_t value);

/* PCB Call State Access */
DomainCallState nd500_domain_read_call_state(Nd500Cpu* cpu, uint8_t domain);
void nd500_domain_write_call_state(Nd500Cpu* cpu, uint8_t domain, DomainCallState state);

/* Domain Analysis */
uint8_t nd500_domain_from_address(Nd500Cpu* cpu, uint32_t virtual_addr);
int nd500_domain_is_cross_domain_call(Nd500Cpu* cpu, uint32_t target_addr);

/* Domain Validation */
int nd500_domain_is_valid(uint8_t domain);
int nd500_domain_has_capability(Nd500Cpu* cpu, uint8_t domain, int segment);
