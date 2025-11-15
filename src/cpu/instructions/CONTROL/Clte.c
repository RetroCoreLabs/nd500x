#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Clte instruction - CONTROL class
 *
 * CLTE - Clear Trap Enable (Clear bit in trap enable register)
 *
 * Mnemonic: CLTE
 * Format: CLTE <bit_number/r/BY>
 * Variants: 1
 * Operands: 1 (<bit_number/r/BY>)
 *
 * Opcode:
 *   0xFD3A (CLTE) - Clear bit in trap enable register
 *
 * Operation:
 *   trap_enable_register[bit_number] = 0
 *
 * Description:
 *   Clears (disables) a specific bit in the trap enable register, disabling
 *   the corresponding trap type. The trap enable register controls which
 *   types of traps (exceptions) are enabled and will cause trap handling
 *   when they occur.
 *
 *   On the ND-500, the trap enable register is part of the domain system
 *   and consists of:
 *   - OTE (Own Trap Enable): Controls traps for the current domain
 *   - MTE (Mother Trap Enable): Controls traps for parent domain
 *
 *   CLTE operates on the OTE register, disabling specific trap types
 *   for the currently executing domain. This allows software to
 *   selectively disable exception handling for different conditions,
 *   typically during critical sections or when exceptions are not desired.
 *
 * Bit Number Range:
 *   - Valid range: 0 to 15 (16 trap enable bits)
 *   - Each bit corresponds to a specific trap type
 *   - Values outside range may cause IOV (Illegal Operand Value) trap
 *
 * Trap Enable Bit Mapping (Common):
 *   Bit 0:  Arithmetic overflow (OV)
 *   Bit 1:  Divide by zero (DZ)
 *   Bit 2:  Illegal instruction code (IIC)
 *   Bit 3:  Illegal operand value (IOV)
 *   Bit 4:  Addressing trap (AT)
 *   Bit 5:  Breakpoint trap (BPT)
 *   Bit 6:  Trace trap (TRA)
 *   Bit 7:  Single step trap (SST)
 *   Bit 8:  Branch trap (BT)
 *   Bit 9:  Floating underflow (FU)
 *   Bit 10: Floating overflow (FO)
 *   Bit 11-15: System-specific or reserved
 *
 *   (Exact mapping is system-dependent; consult architecture manual)
 *
 * Flags: None modified
 *   All status flags remain unchanged by this instruction
 *
 * Trap conditions:
 *   - IOV (Illegal Operand Value) if bit_number out of range
 *   - Privilege violation if not in supervisor/kernel mode (system-dependent)
 *
 * Performance:
 *   - Execution: 2-3 cycles
 *   - Minimal overhead (bit clear operation)
 *
 * Key Characteristics:
 *   - Single byte operand (bit number 0-15)
 *   - Modifies trap enable register (OTE)
 *   - Disables specific trap types
 *   - Complement of SETE instruction
 *   - Privileged operation (may require supervisor mode)
 *   - No flags modified
 *   - Essential for critical section protection
 *
 * Common Use Cases:
 *   - Disable overflow traps in performance-critical code
 *   - Disable divide-by-zero during error recovery
 *   - Disable breakpoint traps in production code
 *   - Disable trace traps after debugging
 *   - Critical section protection (disable traps temporarily)
 *   - Operating system exception management
 *   - Temporary exception suppression
 *
 * Example Usage:
 *   ; Disable overflow trap for performance
 *   BY = 0                ; Bit 0: Overflow trap
 *   CLTE BY               ; Disable OV trap
 *
 *   ; Disable divide-by-zero trap
 *   BY = 1                ; Bit 1: Divide by zero
 *   CLTE BY               ; Disable DZ trap
 *
 *   ; Disable breakpoint trap (production mode)
 *   CLTE 5                ; Bit 5: BPT trap
 *
 *   ; Disable multiple traps
 *   CLTE 0                ; Disable OV
 *   CLTE 1                ; Disable DZ
 *   CLTE 2                ; Disable IIC
 *
 *   ; Disable trace trap after debugging
 *   CLTE 6                ; Bit 6: Trace trap
 *   ; Normal execution resumes without traps
 *
 *   ; Fast calculation with traps disabled
 *   CLTE 0                ; Disable OV trap
 *   I1 = LARGE_CALC_1
 *   I2 = LARGE_CALC_2
 *   I3 = I1 + I2          ; May overflow, but no trap
 *   SETE 0                ; Re-enable OV trap
 *
 * Related Instructions:
 *   - SETE: Set trap enable bit (enable trap)
 *   - BP: Breakpoint (causes BPT trap if enabled)
 *   - DIVIDE: Can cause DZ trap if enabled
 *
 * Comparison with Related Instructions:
 *   - CLTE vs SETE: Disables trap vs enables trap
 *   - CLTE vs interrupt disable: Different mechanisms (traps vs interrupts)
 *   - CLTE vs status flag clear: Modifies trap enable, not status flags
 *
 * Typical Pattern (Critical Section with Traps):
 *   ; Save current trap enable state (if needed)
 *   ; Disable traps during critical section
 *   CLTE 0                ; Disable OV trap
 *   CLTE 1                ; Disable DZ trap
 *   ; Critical section code (no trap handling overhead)
 *   CALCULATE_VALUES
 *   ; Re-enable traps
 *   SETE 0                ; Re-enable OV trap
 *   SETE 1                ; Re-enable DZ trap
 *
 * Production vs Debug Pattern:
 *   ; Production mode: disable debug traps
 *   CLTE 5                ; Disable breakpoint trap
 *   CLTE 6                ; Disable trace trap
 *   ; Production code runs without debug overhead
 *   ; ...
 *   ; Debug mode: enable debug traps
 *   SETE 5                ; Enable BPT
 *   SETE 6                ; Enable trace
 *
 * Performance Critical Loop:
 *   ; Disable traps for tight loop
 *   CLTE 0                ; Disable OV
 *   LOOP_START:
 *       ; High-performance calculations
 *       FAST_CALC
 *       W LOOPD:B I1, 1, LOOP_START
 *   ; Re-enable traps after loop
 *   SETE 0                ; Re-enable OV
 *
 * Privilege Considerations:
 *   CLTE is typically a privileged instruction requiring supervisor
 *   or kernel mode. User-mode code attempting to execute CLTE may
 *   cause a privilege violation trap. This prevents user code from
 *   disabling critical system traps that ensure system stability.
 *
 * Trap Suppression Risks:
 *   Disabling traps can hide serious errors:
 *   - Overflow traps disabled: Silent arithmetic errors
 *   - Divide-by-zero disabled: Invalid results propagate
 *   - Addressing traps disabled: Memory corruption possible
 *   Use CLTE carefully and re-enable traps as soon as possible.
 *
 * OTE vs MTE:
 *   - OTE (Own Trap Enable): Controls traps for current domain
 *   - MTE (Mother Trap Enable): Controls traps for parent domain
 *   - CLTE modifies OTE
 *   - Some systems may have instructions to modify MTE
 *
 * Multiple Domain Systems:
 *   In multi-domain systems, each domain has its own OTE register.
 *   CLTE modifies the OTE for the currently active domain. Domain
 *   switching preserves OTE state, so disabled traps remain disabled
 *   until explicitly re-enabled with SETE.
 *
 * Boot-Time Trap Configuration:
 *   At system boot, all traps are typically disabled. Operating system
 *   initialization code uses SETE to enable required traps. Application
 *   code may use CLTE/SETE to manage traps dynamically based on needs.
 *
 * Performance vs Safety Trade-off:
 *   Disabling traps improves performance by eliminating trap handling
 *   overhead, but reduces error detection. Choose wisely:
 *   - Performance-critical code: Consider disabling overflow traps
 *   - Safety-critical code: Keep all traps enabled
 *   - Production code: Disable debug traps (BPT, TRA, SST)
 *   - Development code: Enable all traps for debugging
 *
 * Interrupt vs Trap:
 *   CLTE disables traps (synchronous exceptions caused by instructions),
 *   not interrupts (asynchronous events from hardware). Interrupt enable
 *   is controlled separately (system-dependent mechanism).
 *
 * Nesting CLTE/SETE:
 *   When nesting CLTE/SETE operations (e.g., function calls that modify
 *   trap enables), consider saving and restoring OTE state:
 *   ; Function entry
 *   SAVE_OTE_STATE        ; Save current trap enable
 *   CLTE 0                ; Function needs traps disabled
 *   ; Function body
 *   RESTORE_OTE_STATE     ; Restore caller's trap state
 *
 * Implementation Notes:
 *   In a full implementation, this instruction would:
 *   1. Validate bit_number is in range (0-15)
 *   2. Check privilege level (may require supervisor mode)
 *   3. Read current OTE register value
 *   4. Clear the specified bit (bit_number)
 *   5. Write updated value back to OTE
 *   6. Subsequent trap conditions for that type are ignored
 *
 *   In an emulator without full trap system, this would:
 *   - Track trap enable state in CPU structure
 *   - Check enabled traps when trap conditions occur
 *   - Suppress trap handlers for disabled trap types
 */
void nd500_instr_Clte(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count
    if (fi->operand_count != 1) {
        printf("[ERROR] CLTE at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Read bit number from operand (always byte-sized)
    uint64_t bit_number_raw = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_BYTE);
    uint32_t bit_number = (uint32_t)(bit_number_raw & 0xFF);

    // Validate bit number range (0-15 for trap enable register)
    if (bit_number > 15) {
        printf("[ERROR] CLTE at PC=0x%08X: Bit number %u out of range (must be 0-15)\n",
               fi->address, bit_number);
        trap_invalid_operation(cpu, fi->address);
        return;
    }

    // TODO: When trap enable register system is implemented:
    // cpu->OTE &= ~(1U << bit_number);  // Clear bit in Own Trap Enable register
    //
    // The trap system would then check cpu->OTE before raising traps
    //
    // For now, just log the trap disable operation
    printf("[CLTE] Disabled trap bit %u at PC=0x%08X\n", bit_number, fi->address);

    // No status flags are modified by CLTE instruction
}
