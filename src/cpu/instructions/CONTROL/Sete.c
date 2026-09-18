/*
 * Sete.c - ND-500 Sete instruction (CONTROL class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "instructions_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Sete instruction - CONTROL class
 *
 * SETE - Set Trap Enable (Set bit in trap enable register)
 *
 * Mnemonic: SETE
 * Format: SETE <bit_number/r/BY>
 * Variants: 1
 * Operands: 1 (<bit_number/r/BY>)
 *
 * Opcode:
 *   0xFD39 (SETE) - Set bit in trap enable register
 *
 * Operation:
 *   trap_enable_register[bit_number] = 1
 *
 * Description:
 *   Sets (enables) a specific bit in the trap enable register, enabling
 *   the corresponding trap type. The trap enable register controls which
 *   types of traps (exceptions) are enabled and will cause trap handling
 *   when they occur.
 *
 *   On the ND-500, the trap enable register is part of the domain system
 *   and consists of:
 *   - OTE (Own Trap Enable): Controls traps for the current domain
 *   - MTE (Mother Trap Enable): Controls traps for parent domain
 *
 *   SETE operates on the OTE register, enabling specific trap types
 *   for the currently executing domain. This allows software to
 *   selectively enable exception handling for different conditions.
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
 *   - Minimal overhead (bit set operation)
 *
 * Key Characteristics:
 *   - Single byte operand (bit number 0-15)
 *   - Modifies trap enable register (OTE)
 *   - Enables specific trap types
 *   - Complement of CLTE instruction
 *   - Privileged operation (may require supervisor mode)
 *   - No flags modified
 *   - Essential for exception handling configuration
 *
 * Common Use Cases:
 *   - Enable arithmetic overflow traps for critical calculations
 *   - Enable divide-by-zero detection in safe code
 *   - Enable breakpoint traps for debugging
 *   - Enable trace traps for single-stepping
 *   - Enable addressing traps for memory protection
 *   - Selectively enable exception handling
 *   - Operating system trap configuration
 *
 * Example Usage:
 *   ; Enable overflow trap
 *   BY = 0                ; Bit 0: Overflow trap
 *   SETE BY               ; Enable OV trap
 *
 *   ; Enable divide-by-zero trap
 *   BY = 1                ; Bit 1: Divide by zero
 *   SETE BY               ; Enable DZ trap
 *
 *   ; Enable breakpoint trap (for debugger)
 *   SETE 5                ; Bit 5: BPT trap
 *
 *   ; Enable multiple traps
 *   SETE 0                ; Enable OV
 *   SETE 1                ; Enable DZ
 *   SETE 2                ; Enable IIC
 *
 *   ; Enable trace trap for debugging
 *   SETE 6                ; Bit 6: Trace trap
 *   ; Now every instruction will trap after execution
 *
 *   ; Safe division with trap enabled
 *   SETE 1                ; Enable DZ trap
 *   I1 = NUMERATOR
 *   I2 = DENOMINATOR
 *   I3 = I1 / I2          ; If I2=0, trap will occur
 *   CLTE 1                ; Disable DZ trap
 *
 * Related Instructions:
 *   - CLTE: Clear trap enable bit (disable trap)
 *   - BP: Breakpoint (causes BPT trap if enabled)
 *   - DIVIDE: Can cause DZ trap if enabled
 *
 * Comparison with Related Instructions:
 *   - SETE vs CLTE: Enables trap vs disables trap
 *   - SETE vs interrupt enable: Different mechanisms (traps vs interrupts)
 *   - SETE vs status flag set: Modifies trap enable, not status flags
 *
 * Typical Pattern (Critical Section with Traps):
 *   ; Disable traps during critical section
 *   CLTE 0                ; Disable OV trap
 *   CLTE 1                ; Disable DZ trap
 *   ; Critical section code (no trap handling)
 *   CALCULATE_VALUES
 *   ; Re-enable traps
 *   SETE 0                ; Re-enable OV trap
 *   SETE 1                ; Re-enable DZ trap
 *
 * Debugging Pattern:
 *   ; Enable all debug traps
 *   SETE 5                ; Breakpoint trap
 *   SETE 6                ; Trace trap
 *   ; Debugging code
 *   ; ...
 *   ; Disable debug traps
 *   CLTE 5                ; Disable BPT
 *   CLTE 6                ; Disable trace
 *
 * Privilege Considerations:
 *   SETE is typically a privileged instruction requiring supervisor
 *   or kernel mode. User-mode code attempting to execute SETE may
 *   cause a privilege violation trap. This prevents user code from
 *   disabling critical system traps.
 *
 * Trap Handler Registration:
 *   SETE only enables the trap; the trap handler address must be
 *   configured separately (typically in the domain's trap vector
 *   table or system control registers).
 *
 * OTE vs MTE:
 *   - OTE (Own Trap Enable): Controls traps for current domain
 *   - MTE (Mother Trap Enable): Controls traps for parent domain
 *   - SETE modifies OTE
 *   - Some systems may have instructions to modify MTE
 *
 * Multiple Domain Systems:
 *   In multi-domain systems, each domain has its own OTE register.
 *   SETE modifies the OTE for the currently active domain. Domain
 *   switching preserves OTE state.
 *
 * Trap Enable at Boot:
 *   Typically, all traps are disabled at system boot. The boot code
 *   or operating system initialization uses SETE to enable required
 *   traps (e.g., page faults, illegal instructions, arithmetic errors).
 *
 * Performance Impact:
 *   Enabling traps may have performance impact if trap conditions
 *   occur frequently. For example, enabling overflow trap in tight
 *   loops with frequent overflows can significantly reduce performance
 *   due to trap handling overhead.
 *
 * Implementation Notes:
 *   In a full implementation, this instruction would:
 *   1. Validate bit_number is in range (0-15)
 *   2. Check privilege level (may require supervisor mode)
 *   3. Read current OTE register value
 *   4. Set the specified bit (bit_number)
 *   5. Write updated value back to OTE
 *   6. Subsequent instructions can now trap on that condition
 *
 *   In an emulator without full trap system, this would:
 *   - Track trap enable state in CPU structure
 *   - Check enabled traps when trap conditions occur
 *   - Invoke trap handlers only for enabled traps
 */
void nd500_instr_Sete(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count
    if (fi->operand_count != 1) {
        printf("[ERROR] SETE at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Read bit number from operand (always byte-sized)
    uint64_t bit_number_raw = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_BYTE);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }
    uint32_t bit_number = (uint32_t)(bit_number_raw & 0xFF);

    // Validate bit number range (0-63 for 64-bit trap enable register)
    if (bit_number > 63) {
        printf("[ERROR] SETE at PC=0x%08X: Bit number %u out of range (must be 0-63)\n",
               fi->address, bit_number);
        raise_trap(cpu, TRAP_IOV, fi->address, 0);
        return;
    }

    // TEMM gating (manual 10.24, ref line 10177): the bit is modifiable only if
    // the corresponding TEMM bit is set, else an illegal operand value trap.
    uint32_t bit_in_half = 1U << (bit_number & 31);
    uint32_t temm_half = (bit_number < 32) ? cpu->TEMM1 : cpu->TEMM2;
    if (!nd500_temm_allows_change(bit_in_half, temm_half)) {
        raise_trap(cpu, TRAP_IOV, fi->address, 0);
        return;
    }

    // Set bit in Own Trap Enable register (OTE1 for bits 0-31, OTE2 for bits 32-63)
    if (bit_number < 32) {
        cpu->OTE1 |= (1U << bit_number);
    } else {
        cpu->OTE2 |= (1U << (bit_number - 32));
    }

    printf("[SETE] Enabled trap bit %u (OTE=0x%08X%08X) at PC=0x%08X\n",
           bit_number, cpu->OTE2, cpu->OTE1, fi->address);

    // No status flags are modified by SETE instruction
}
