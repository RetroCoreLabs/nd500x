#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * IfStackGo instruction - BRANCH class
 *
 * IF -ST GO - If Status Bit NOT Set, Go (Conditional branch on status bit clear)
 *
 * Mnemonic: IF ST GO
 * Format: IF ST GO <bit_number>, <<displacement>>
 * Variants: 2
 * Operands: 2 (<bit_number/r/BY>, <<displacement>>)
 *
 * Opcodes:
 *   0xFC7B (IF ST GO:B) - Branch with byte displacement if status bit set
 *   0xFD64 (IF ST GO:H) - Branch with halfword displacement if status bit set
 *
 * Operation:
 *   if (status_register[bit_number] == 1) then
 *       PC = PC + sign_extend(displacement)
 *   endif
 *
 * Description:
 *   Performs a conditional branch if the specified bit in the status register
 *   is set (equals 1). The bit_number operand specifies which bit to test,
 *   with valid values from 0 to 29 inclusive.
 *
 *   The status register on the ND-500 consists of ST1 (bits 0-31) and ST2
 *   (bits 32-63), forming a 64-bit status register. This instruction tests
 *   bits 0-29, which all reside in the ST1 register.
 *
 *   This instruction enables sophisticated control flow based on arbitrary
 *   status conditions beyond the standard flag bits (C, Z, S, V, K). Programs
 *   can use custom status bits for application-specific conditions, mode
 *   flags, or system state tracking.
 *
 * Bit Number Range:
 *   - Valid range: 0 to 29 inclusive (30 bits)
 *   - All bits tested are in ST1 register
 *   - Values outside this range cause IOV (Illegal Operand Value) trap
 *
 * Displacement Encoding:
 *   - Byte displacement (0xFC7B): Signed 8-bit (-128 to +127 bytes)
 *   - Halfword displacement (0xFD64): Signed 16-bit (-32768 to +32767 bytes)
 *   - Assembler auto-selects optimal size based on target distance
 *
 * Flags: None modified
 *   All status flags remain unchanged by this instruction
 *
 * Trap conditions:
 *   - IOV (Illegal Operand Value) if bit_number > 29
 *   - Addressing traps for operand access
 *   - BT (Branch Trap) if target address protection violation
 *   - Page fault if target page not present
 *
 * Performance:
 *   - Branch taken: 2-3 cycles
 *   - Branch not taken: 1-2 cycles
 *
 * Key Characteristics:
 *   - Tests arbitrary status register bits (not just standard flags)
 *   - Bit range 0-29 (30 bits available)
 *   - PC-relative branching (not absolute)
 *   - Two displacement sizes for short/long jumps
 *   - Enables custom application status bits
 *   - Complements standard flag-based branches
 *
 * Common Use Cases:
 *   - Testing application-specific mode flags
 *   - Checking system state conditions
 *   - Multi-level interrupt enable/disable testing
 *   - Debug mode flag checking
 *   - Privilege level validation
 *   - Feature enable/disable flag testing
 *   - Custom condition code testing
 *
 * Example Usage:
 *   ; Test if custom mode bit (bit 20) is active
 *   IF ST GO:B 20, MODE_ACTIVE    ; Branch if bit 20 set
 *   ; Mode not active
 *   ...
 *   MODE_ACTIVE:
 *   ; Mode is active
 *
 *   ; Test debug flag (bit 15)
 *   IF ST GO:H 15, DEBUG_HANDLER  ; Branch if debug mode enabled
 *   ; Normal execution
 *   ...
 *   DEBUG_HANDLER:
 *   ; Debug mode active
 *
 *   ; Test interrupt enable (bit 25)
 *   IF ST GO:B 25, INTERRUPTS_ON
 *   ; Interrupts disabled
 *   JMP POLL_MODE
 *   INTERRUPTS_ON:
 *   ; Wait for interrupt
 *
 * Related Instructions:
 *   - IF -ST GO: Branch if status bit NOT set
 *   - IF=GO, IF<>GO: Branch on zero flag
 *   - IF<GO, IF>GO: Branch on comparison results
 *   - GO: Unconditional relative branch
 *
 * Comparison with Related Instructions:
 *   - IF ST GO vs IF -ST GO: Tests bit set vs bit clear
 *   - IF ST GO vs IF=GO: Arbitrary bit vs Z flag only
 *   - IF ST GO vs IFKGO: Status bit vs K flag bit
 *
 * Status Register Organization:
 *   The ND-500 status register is 64 bits:
 *   - ST1 (bits 0-31): Data status flags, system flags
 *   - ST2 (bits 32-63): Extended system status
 *   - This instruction tests bits 0-29 (in ST1)
 *   - Bits 30-31 reserved or used for other purposes
 *
 * Standard Flag Bits (subset of tested range):
 *   Common standard flags that may be tested:
 *   - Bit 5: Z (Zero flag)
 *   - Bit 6: C (Carry/borrow flag)
 *   - Bit 7: S (Sign flag)
 *   - Bit 8: K (User flag)
 *   - Bit 9: V (Overflow flag)
 *   However, IF ST GO can test ANY bit 0-29, not just these.
 */
void nd500_instr_IfStackGo(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    // Validate operand count
    if (fi->operand_count != 2) {
        printf("[ERROR] IF ST GO at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Read bit number from first operand (always byte-sized)
    uint64_t bit_number_raw = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_BYTE);
    uint32_t bit_number = (uint32_t)(bit_number_raw & 0xFF);

    // Validate bit number range (0-29 inclusive)
    if (bit_number > 29) {
        printf("[ERROR] IF ST GO at PC=0x%08X: Bit number %u out of range (must be 0-29)\n",
               fi->address, bit_number);
        trap_invalid_operation(cpu, fi->address);
        return;
    }

    // Test if specified bit in status register (ST1) is NOT set
    bool bit_is_set = (cpu->ST1 & (1U << bit_number)) != 0;

    if (!bit_is_set) {
        // Read displacement value and sign-extend based on data type
        uint64_t value = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);
        int64_t displacement = nd500_sign_extend_by_dtype(value, fi->data_type);

        // Update PC (relative branch from instruction start)
        cpu->PC = (uint32_t)(fi->address + displacement);

        // Set Branch Trap (BT) bit in status register when branch is taken
        cpu->ST1 |= 0x40000;  // BT bit = bit 18
    }
    // else: bit is set, branch not taken, PC already points to next instruction

    // No status flags are modified by this instruction
}
