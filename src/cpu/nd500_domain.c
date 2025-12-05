#include "nd500_domain.h"
#include "nd500_mmu.h"
#include "cpu_protos.h"
#include "../machine/machine_protos.h"
#include <stdio.h>
#include <string.h>

/*
 * ND-500 Domain System Implementation
 * Based on C# RetroCore emulator CpuND500.Domain.cs
 *
 * Provides cross-domain calling for kernel/user separation and
 * process isolation on the ND-500 architecture.
 */

// ═══════════════════════════════════════════════════════
// DOMAIN SYSTEM INITIALIZATION
// ═══════════════════════════════════════════════════════

/**
 * Initialize domain system
 * Sets initial domain to 0 (kernel domain)
 */
void nd500_domain_init(Nd500Cpu* cpu) {
    if (!cpu) return;

    /* Start in domain 0 (kernel) */
    cpu->CED = KERNEL_DOMAIN;  /* Current Executing Domain */
    cpu->CAD = KERNEL_DOMAIN;  /* Current Alternative Domain */
}

/**
 * Setup Domain Information Table in memory
 * Allocates DIT at specified physical address
 *
 * @param cpu CPU structure
 * @param ditbase Physical address for DIT (must be aligned)
 */
void nd500_domain_setup_dit(Nd500Cpu* cpu, uint32_t ditbase) {
    if (!cpu || !cpu->machine) return;

    /* Store DIT base address in CPU register */
    cpu->DITBASE = ditbase;

    /* Clear DIT memory (256 domains * 16 bytes = 4096 bytes) */
    uint32_t dit_size = MAX_DOMAINS * DIT_ENTRY_SIZE;
    for (uint32_t i = 0; i < dit_size; i++) {
        nd500_bus_write8(cpu->machine, ditbase + i, 0);
    }

    printf("ND-500: DIT setup at 0x%08X (%d domains, %d bytes)\n",
           ditbase, MAX_DOMAINS, dit_size);
}

// ═══════════════════════════════════════════════════════
// DIT ACCESS FUNCTIONS
// ═══════════════════════════════════════════════════════

/**
 * Read Top of Stack for specified domain from DIT
 */
uint32_t nd500_domain_read_tos(Nd500Cpu* cpu, uint8_t domain) {
    if (!cpu || !cpu->machine || domain >= MAX_DOMAINS) return 0;

    uint32_t dit_entry_addr = cpu->DITBASE + (domain * DIT_ENTRY_SIZE) + DIT_TOS_OFFSET;
    return nd500_bus_read32(cpu->machine, dit_entry_addr);
}

/**
 * Read Lower Limit for specified domain from DIT
 */
uint32_t nd500_domain_read_ll(Nd500Cpu* cpu, uint8_t domain) {
    if (!cpu || !cpu->machine || domain >= MAX_DOMAINS) return 0;

    uint32_t dit_entry_addr = cpu->DITBASE + (domain * DIT_ENTRY_SIZE) + DIT_LL_OFFSET;
    return nd500_bus_read32(cpu->machine, dit_entry_addr);
}

/**
 * Read Higher Limit for specified domain from DIT
 */
uint32_t nd500_domain_read_hl(Nd500Cpu* cpu, uint8_t domain) {
    if (!cpu || !cpu->machine || domain >= MAX_DOMAINS) return 0;

    uint32_t dit_entry_addr = cpu->DITBASE + (domain * DIT_ENTRY_SIZE) + DIT_HL_OFFSET;
    return nd500_bus_read32(cpu->machine, dit_entry_addr);
}

/**
 * Read Trap Handler Address for specified domain from DIT
 */
uint32_t nd500_domain_read_tha(Nd500Cpu* cpu, uint8_t domain) {
    if (!cpu || !cpu->machine || domain >= MAX_DOMAINS) return 0;

    uint32_t dit_entry_addr = cpu->DITBASE + (domain * DIT_ENTRY_SIZE) + DIT_THA_OFFSET;
    return nd500_bus_read32(cpu->machine, dit_entry_addr);
}

/**
 * Write Top of Stack for specified domain to DIT
 */
void nd500_domain_write_tos(Nd500Cpu* cpu, uint8_t domain, uint32_t value) {
    if (!cpu || !cpu->machine || domain >= MAX_DOMAINS) return;

    uint32_t dit_entry_addr = cpu->DITBASE + (domain * DIT_ENTRY_SIZE) + DIT_TOS_OFFSET;
    nd500_bus_write32(cpu->machine, dit_entry_addr, value);
}

/**
 * Write Lower Limit for specified domain to DIT
 */
void nd500_domain_write_ll(Nd500Cpu* cpu, uint8_t domain, uint32_t value) {
    if (!cpu || !cpu->machine || domain >= MAX_DOMAINS) return;

    uint32_t dit_entry_addr = cpu->DITBASE + (domain * DIT_ENTRY_SIZE) + DIT_LL_OFFSET;
    nd500_bus_write32(cpu->machine, dit_entry_addr, value);
}

/**
 * Write Higher Limit for specified domain to DIT
 */
void nd500_domain_write_hl(Nd500Cpu* cpu, uint8_t domain, uint32_t value) {
    if (!cpu || !cpu->machine || domain >= MAX_DOMAINS) return;

    uint32_t dit_entry_addr = cpu->DITBASE + (domain * DIT_ENTRY_SIZE) + DIT_HL_OFFSET;
    nd500_bus_write32(cpu->machine, dit_entry_addr, value);
}

/**
 * Write Trap Handler Address for specified domain to DIT
 */
void nd500_domain_write_tha(Nd500Cpu* cpu, uint8_t domain, uint32_t value) {
    if (!cpu || !cpu->machine || domain >= MAX_DOMAINS) return;

    uint32_t dit_entry_addr = cpu->DITBASE + (domain * DIT_ENTRY_SIZE) + DIT_THA_OFFSET;
    nd500_bus_write32(cpu->machine, dit_entry_addr, value);
}

// ═══════════════════════════════════════════════════════
// PCB CALL STATE ACCESS
// ═══════════════════════════════════════════════════════

/**
 * Read call state from PCB for specified domain
 * Returns the saved caller context from a cross-domain call
 */
DomainCallState nd500_domain_read_call_state(Nd500Cpu* cpu, uint8_t domain) {
    DomainCallState state = {0, 0, 0, 0};

    if (!cpu || !cpu->machine || domain >= MAX_DOMAINS) {
        return state;
    }

    /* Calculate PCB address in memory */
    uint32_t pcb_base = cpu->PS + (domain * PCB_SIZE);
    uint32_t call_state_addr = pcb_base + PCB_CALL_OFFSET;

    /* Read call state structure from memory */
    state.calling_domain = nd500_bus_read8(cpu->machine, call_state_addr + PCB_CALL_CE_OFFSET);
    state.alternative_domain = nd500_bus_read8(cpu->machine, call_state_addr + PCB_CALL_CA_OFFSET);
    state.calling_p = nd500_bus_read32(cpu->machine, call_state_addr + PCB_CALL_P_OFFSET);
    state.calling_b = nd500_bus_read32(cpu->machine, call_state_addr + PCB_CALL_B_OFFSET);

    return state;
}

/**
 * Write call state to PCB for specified domain
 * Saves caller context for later return from cross-domain call
 */
void nd500_domain_write_call_state(Nd500Cpu* cpu, uint8_t domain, DomainCallState state) {
    if (!cpu || !cpu->machine || domain >= MAX_DOMAINS) {
        return;
    }

    /* Calculate PCB address in memory */
    uint32_t pcb_base = cpu->PS + (domain * PCB_SIZE);
    uint32_t call_state_addr = pcb_base + PCB_CALL_OFFSET;

    /* Write call state structure to memory */
    nd500_bus_write8(cpu->machine, call_state_addr + PCB_CALL_CE_OFFSET, state.calling_domain);
    nd500_bus_write8(cpu->machine, call_state_addr + PCB_CALL_CA_OFFSET, state.alternative_domain);
    nd500_bus_write32(cpu->machine, call_state_addr + PCB_CALL_P_OFFSET, state.calling_p);
    nd500_bus_write32(cpu->machine, call_state_addr + PCB_CALL_B_OFFSET, state.calling_b);
}

// ═══════════════════════════════════════════════════════
// DOMAIN STATE MANAGEMENT
// ═══════════════════════════════════════════════════════

/**
 * Save current CPU state to DIT for specified domain
 * Stores TOS, LL, HL, THA registers
 */
void nd500_domain_save_state(Nd500Cpu* cpu, uint8_t domain) {
    if (!cpu || !cpu->machine || domain >= MAX_DOMAINS) return;

    nd500_domain_write_tos(cpu, domain, cpu->TOS);
    nd500_domain_write_ll(cpu, domain, cpu->LL);
    nd500_domain_write_hl(cpu, domain, cpu->HL);
    nd500_domain_write_tha(cpu, domain, cpu->THA);
}

/**
 * Load CPU state from DIT for specified domain
 * Restores TOS, LL, HL, THA registers
 */
void nd500_domain_load_state(Nd500Cpu* cpu, uint8_t domain) {
    if (!cpu || !cpu->machine || domain >= MAX_DOMAINS) return;

    cpu->TOS = nd500_domain_read_tos(cpu, domain);
    cpu->LL = nd500_domain_read_ll(cpu, domain);
    cpu->HL = nd500_domain_read_hl(cpu, domain);
    cpu->THA = nd500_domain_read_tha(cpu, domain);
}

// ═══════════════════════════════════════════════════════
// CROSS-DOMAIN CALLING
// ═══════════════════════════════════════════════════════

/**
 * Check if current stack frame marks a domain boundary
 * Domain boundary is marked by PREVB=0 and RETA=0 on stack
 *
 * @return 1 if at domain boundary, 0 otherwise
 */
int nd500_domain_is_boundary(Nd500Cpu* cpu) {
    if (!cpu || !cpu->machine) return 0;

    /* Read PREVB and RETA from stack at B-8 and B-4 */
    uint32_t prevb = nd500_bus_read32(cpu->machine, cpu->B - 8);
    uint32_t reta = nd500_bus_read32(cpu->machine, cpu->B - 4);

    return (prevb == PREVB_MARKER && reta == RETA_MARKER);
}

/**
 * Perform cross-domain call
 *
 * This implements the ND-500 cross-domain calling mechanism:
 * 1. Save caller context (CED, CAD, PC, B) to TARGET domain's PCB
 * 2. Save current domain state (TOS, LL, HL, THA) to DIT
 * 3. Update domain registers (CAD, CED)
 * 4. Load target domain state from DIT
 * 5. Mark domain boundary on stack (PREVB=0, RETA=0)
 * 6. Update B register for new stack frame
 *
 * @param cpu CPU structure
 * @param target_domain Domain to switch to (0-255)
 * @param entry_point Virtual address to begin execution in target domain
 */
void nd500_domain_switch(Nd500Cpu* cpu, uint8_t target_domain, uint32_t entry_point) {
    if (!cpu || !cpu->machine) return;

    /* Validate target domain */
    if (target_domain >= MAX_DOMAINS) {
        fprintf(stderr, "ND-500: Invalid domain %d (max %d)\n", target_domain, MAX_DOMAINS - 1);
        return;
    }

    uint8_t calling_domain = (uint8_t)cpu->CED;

    /* Don't do anything if switching to same domain */
    if (target_domain == calling_domain) {
        cpu->PC = entry_point;
        return;
    }

    /* ─────────────────────────────────────────────────────────
     * STEP 1: Save caller context to TARGET domain's PCB
     * ─────────────────────────────────────────────────────── */

    DomainCallState call_state;
    call_state.calling_domain = calling_domain;
    call_state.alternative_domain = (uint8_t)cpu->CAD;
    call_state.calling_p = cpu->PC;
    call_state.calling_b = cpu->B;

    nd500_domain_write_call_state(cpu, target_domain, call_state);

    /* ─────────────────────────────────────────────────────────
     * STEP 2: Save current domain state to DIT
     * ─────────────────────────────────────────────────────── */

    nd500_domain_save_state(cpu, calling_domain);

    /* ─────────────────────────────────────────────────────────
     * STEP 3: Update domain registers
     * ─────────────────────────────────────────────────────── */

    cpu->CED = target_domain;  /* Update Current Executing Domain */
    cpu->CAD = target_domain;  /* Update Current Alternative Domain */

    /* ─────────────────────────────────────────────────────────
     * STEP 4: Load target domain state from DIT
     * ─────────────────────────────────────────────────────── */

    nd500_domain_load_state(cpu, target_domain);

    /* ─────────────────────────────────────────────────────────
     * STEP 5: Mark domain boundary on stack
     * Write PREVB=0 and RETA=0 markers
     * ─────────────────────────────────────────────────────── */

    /* Allocate stack frame for domain boundary markers */
    cpu->B = cpu->TOS;  /* New B points to current TOS */
    cpu->TOS = cpu->B + 16;  /* Allocate 16 bytes (PREVB, RETA, plus padding) */

    /* Write domain boundary markers */
    nd500_bus_write32(cpu->machine, cpu->B - 8, PREVB_MARKER);  /* PREVB = 0 */
    nd500_bus_write32(cpu->machine, cpu->B - 4, RETA_MARKER);   /* RETA = 0 */

    /* ─────────────────────────────────────────────────────────
     * STEP 6: Update PC to entry point
     * ─────────────────────────────────────────────────────── */

    cpu->PC = entry_point;

    printf("ND-500: Domain switch %d → %d at PC=0x%08X\n",
           calling_domain, target_domain, entry_point);
}

/**
 * Return from cross-domain call
 *
 * Restores caller context from PCB and returns to calling domain:
 * 1. Read caller context from current domain's PCB
 * 2. Save current domain state to DIT
 * 3. Update domain registers (CAD, CED) to caller's values
 * 4. Load calling domain state from DIT
 * 5. Restore PC and B registers
 */
void nd500_domain_return(Nd500Cpu* cpu) {
    if (!cpu || !cpu->machine) return;

    uint8_t current_domain = (uint8_t)cpu->CED;

    /* ─────────────────────────────────────────────────────────
     * STEP 1: Read caller context from PCB
     * ─────────────────────────────────────────────────────── */

    DomainCallState call_state = nd500_domain_read_call_state(cpu, current_domain);

    /* Check if we have a valid caller (call_state won't be all zeros) */
    if (call_state.calling_domain == 0 && call_state.calling_p == 0) {
        fprintf(stderr, "ND-500: Domain return with no caller (domain %d)\n", current_domain);
        return;
    }

    uint8_t calling_domain = call_state.calling_domain;

    /* ─────────────────────────────────────────────────────────
     * STEP 2: Save current domain state to DIT
     * ─────────────────────────────────────────────────────── */

    nd500_domain_save_state(cpu, current_domain);

    /* ─────────────────────────────────────────────────────────
     * STEP 3: Update domain registers to caller's values
     * ─────────────────────────────────────────────────────── */

    cpu->CED = calling_domain;
    cpu->CAD = call_state.alternative_domain;

    /* ─────────────────────────────────────────────────────────
     * STEP 4: Load calling domain state from DIT
     * ─────────────────────────────────────────────────────── */

    nd500_domain_load_state(cpu, calling_domain);

    /* ─────────────────────────────────────────────────────────
     * STEP 5: Restore caller's PC and B
     * ─────────────────────────────────────────────────────── */

    cpu->PC = call_state.calling_p;
    cpu->B = call_state.calling_b;

    /* Clear call state in PCB (mark as returned) */
    DomainCallState empty = {0, 0, 0, 0};
    nd500_domain_write_call_state(cpu, current_domain, empty);

    printf("ND-500: Domain return %d → %d at PC=0x%08X\n",
           current_domain, calling_domain, cpu->PC);
}

// ═══════════════════════════════════════════════════════
// DOMAIN ANALYSIS
// ═══════════════════════════════════════════════════════

/**
 * Determine which domain owns a virtual address
 * Checks all PCB entries to find which domain has capability for this address
 *
 * @param virtual_addr Virtual address to check
 * @return Domain number (0-255) or 0xFF if not found
 */
uint8_t nd500_domain_from_address(Nd500Cpu* cpu, uint32_t virtual_addr) {
    if (!cpu) return 0xFF;

    /* Extract segment from virtual address */
    int segment = (virtual_addr >> 27) & 0x1F;

    /* Check current domain first (most common case) */
    uint8_t current_domain = (uint8_t)cpu->CAD;
    if (nd500_domain_has_capability(cpu, current_domain, segment)) {
        return current_domain;
    }

    /* Search all domains for one with capability for this segment */
    for (int domain = 0; domain < MAX_DOMAINS; domain++) {
        if (nd500_domain_has_capability(cpu, domain, segment)) {
            return (uint8_t)domain;
        }
    }

    return 0xFF;  /* Not found */
}

/**
 * Check if a call to target address would be a cross-domain call
 *
 * @param target_addr Virtual address being called
 * @return 1 if cross-domain call, 0 if same-domain
 */
int nd500_domain_is_cross_domain_call(Nd500Cpu* cpu, uint32_t target_addr) {
    if (!cpu) return 0;

    uint8_t target_domain = nd500_domain_from_address(cpu, target_addr);
    if (target_domain == 0xFF) return 0;  /* Unknown domain */

    return (target_domain != cpu->CED);
}

// ═══════════════════════════════════════════════════════
// DOMAIN VALIDATION
// ═══════════════════════════════════════════════════════

/**
 * Check if domain number is valid
 */
int nd500_domain_is_valid(uint8_t domain) {
    return (domain < MAX_DOMAINS);
}

/**
 * Check if domain has a capability for specified segment
 * Returns 1 if either program or data capability exists
 */
int nd500_domain_has_capability(Nd500Cpu* cpu, uint8_t domain, int segment) {
    if (!cpu || domain >= MAX_DOMAINS || segment < 0 || segment >= MAXSEG) {
        return 0;
    }

    /* Check both program and data capabilities */
    uint16_t prog_cap = nd500_mmu_get_program_capability(cpu, domain, segment);
    uint16_t data_cap = nd500_mmu_get_data_capability(cpu, domain, segment);

    return (prog_cap != 0 || data_cap != 0);
}
