/*
 * ND-500 Indirect Segment Handling
 *
 * Implements cross-domain calls via indirect segment capabilities.
 * Segment 31 is reserved for SINTRAN monitor calls (MON).
 *
 * Reference: ND500_PCB_MMU_COMPLETE_REFERENCE.md
 * C# Reference: CpuND500.IndirectSegments.cs
 */

#ifndef ND500_INDIRECT_H
#define ND500_INDIRECT_H

#include <stdint.h>
#include "cpu_protos.h"

/* Result codes for indirect call handling */
typedef enum {
    INDIRECT_DIRECT = 0,        /* Direct call - no domain switching */
    INDIRECT_HANDLED = 1,       /* Indirect call handled (MON call completed) */
    INDIRECT_DOMAIN_SWITCH = 2, /* Indirect call - domain switch needed */
    INDIRECT_ERROR = -1,        /* Error or halt requested */
    INDIRECT_BREAK = -2,        /* Break into debugger requested */
    INDIRECT_WAIT = -3          /* Blocking read has no input - suspend and retry */
} IndirectCallResult;

/**
 * Check if a call target address is indirect and handle accordingly.
 *
 * This function is called by CALL and CALLG instructions before jumping
 * to the target address. It checks if the target segment has the PC_IND
 * flag set in its program capability.
 *
 * For segment 31 (SINTRAN): The MON call is executed via libmon and
 * execution returns to the caller (no jump to entry point).
 *
 * For other indirect segments: Domain switch is prepared and the resolved
 * entry point is returned (not yet implemented).
 *
 * @param cpu           CPU state
 * @param target_addr   Virtual address from CALL/CALLG instruction
 * @param arg_count     Number of arguments passed
 * @param arg_addresses Array of argument effective addresses
 * @param instruction_addr PC of the CALL instruction (for logging)
 * @param out_resolved  Output: resolved address to jump to (or return addr for MON)
 *
 * @return  INDIRECT_DIRECT (0): Direct call, jump to target_addr
 *          INDIRECT_HANDLED (1): MON call completed, jump to out_resolved (return addr)
 *          INDIRECT_DOMAIN_SWITCH (2): Domain switch needed, jump to out_resolved
 *          INDIRECT_ERROR (-1): Error or halt requested, don't jump
 *          INDIRECT_BREAK (-2): Break requested, don't jump
 */
int nd500_check_indirect_call(
    Nd500Cpu* cpu,
    uint32_t target_addr,
    uint32_t arg_count,
    const uint32_t* arg_addresses,
    uint32_t instruction_addr,
    uint32_t* out_resolved
);

/**
 * Setup segment 31 for SINTRAN MON call interception.
 *
 * Called during DOM/SEG loading to configure the indirect segment
 * capability for segment 31 in a given domain.
 *
 * @param cpu     CPU state
 * @param domain  Domain number to configure
 */
void nd500_setup_sintran_segment(Nd500Cpu* cpu, uint8_t domain);

/**
 * Check if program MMU is enabled.
 * Used to determine if capability lookup is needed.
 */
int nd500_program_mmu_enabled(Nd500Cpu* cpu);

#endif /* ND500_INDIRECT_H */
