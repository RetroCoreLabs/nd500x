#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Ifstgo instruction - BRANCH class
 *
 * IF ST GO - If Status Bit Set, Go (Conditional branch on status bit set)
 *
 * Mnemonic: IF -ST GO
 * Format: IF -ST GO <bit_number>, <<displacement>>
 * Variants: 2
 * Operands: 2 (<bit_number/r/BY>, <<displacement>>)
 *
 * Opcodes:
 *   0xFD65 (IF -ST GO:B) - Branch with byte displacement if status bit NOT set
 *   0xFC84 (IF -ST GO:H) - Branch with halfword displacement if status bit NOT set
 *
 * Operation:
 *   if (status_register[bit_number] == 0) then
 *       PC = PC + sign_extend(displacement)
 *   endif
 *
 * Description:
 *   Performs a conditional branch if the specified bit in the status register
 *   is NOT set (equals 0). The bit_number operand specifies which bit to test,
 *   with valid values from 0 to 29 inclusive.
 *
 *   The status register on the ND-500 consists of ST1 (bits 0-31) and ST2
 *   (bits 32-63), forming a 64-bit status register. This instruction tests
 *   bits 0-29, which all reside in the ST1 register.
 *
 *   This instruction is the complement of IF ST GO, branching when the specified
 *   bit is clear rather than set. It enables testing for disabled conditions,
 *   inactive modes, or cleared status flags.
 *
 * Bit Number Range:
 *   - Valid range: 0 to 29 inclusive (30 bits)
 *   - All bits tested are in ST1 register
 *   - Values outside this range cause IOV (Illegal Operand Value) trap
 *
 * Displacement Encoding:
 *   - Byte displacement (0xFD65): Signed 8-bit (-128 to +127 bytes)
 *   - Halfword displacement (0xFC84): Signed 16-bit (-32768 to +32767 bytes)
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
 *   - Tests for bit clear (NOT set)
 *   - Complement of IF ST GO instruction
 *   - Bit range 0-29 (30 bits available)
 *   - PC-relative branching (not absolute)
 *   - Two displacement sizes for short/long jumps
 *   - Enables negative condition testing
 *   - Essential for disabled-mode checks
 *
 * Common Use Cases:
 *   - Testing if mode flags are disabled
 *   - Checking for inactive system states
 *   - Interrupt disable verification
 *   - Debug mode not active confirmation
 *   - Privilege level checking (bit not set)
 *   - Feature disable flag testing
 *   - Error condition absence verification
 *
 * Example Usage:
 *   ; Test if custom mode bit (bit 20) is inactive
 *   IF -ST GO:B 20, MODE_INACTIVE  ; Branch if bit 20 NOT set
 *   ; Mode is active
 *   ...
 *   MODE_INACTIVE:
 *   ; Mode is inactive
 *
 *   ; Test if debug flag (bit 15) is disabled
 *   IF -ST GO:H 15, NORMAL_MODE    ; Branch if debug mode disabled
 *   ; Debug mode active, special handling
 *   ...
 *   NORMAL_MODE:
 *   ; Normal execution mode
 *
 *   ; Test if interrupt disabled (bit 25 not set)
 *   IF -ST GO:B 25, POLL_MODE
 *   ; Interrupts enabled, wait for interrupt
 *   WAIT
 *   POLL_MODE:
 *   ; Interrupts disabled, use polling
 *
 *   ; Error-free validation
 *   IF -ST GO:B 10, NO_ERROR       ; Branch if error bit not set
 *   ; Error occurred
 *   JMP ERROR_HANDLER
 *   NO_ERROR:
 *   ; Continue normal processing
 *
 * Related Instructions:
 *   - IF ST GO: Branch if status bit IS set
 *   - IF=GO, IF<>GO: Branch on zero flag
 *   - IF<GO, IF>GO: Branch on comparison results
 *   - GO: Unconditional relative branch
 *
 * Comparison with Related Instructions:
 *   - IF -ST GO vs IF ST GO: Tests bit clear vs bit set
 *   - IF -ST GO vs IF=GO: Arbitrary bit vs Z flag only
 *   - IF -ST GO vs IFKGO: Inverted logic, different bits
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
 *   - Bit 5: Z (Zero flag) - test if NOT zero
 *   - Bit 6: C (Carry/borrow flag) - test if no carry
 *   - Bit 7: S (Sign flag) - test if NOT negative
 *   - Bit 8: K (User flag) - test if K clear
 *   - Bit 9: V (Overflow flag) - test if no overflow
 *   However, IF -ST GO can test ANY bit 0-29, not just these.
 *
 * Typical Pattern (Mode Selection):
 *   ; Check if special mode active
 *   IF ST GO:B 15, SPECIAL_MODE    ; Branch if bit 15 set
 *   ; Bit 15 not set, check alternative
 *   IF -ST GO:B 16, DEFAULT_MODE   ; Branch if bit 16 not set
 *   ; Bit 16 is set
 *   JMP MODE_C
 *   SPECIAL_MODE:
 *   ; Bit 15 set, special mode
 *   ...
 *   DEFAULT_MODE:
 *   ; Bits 15 and 16 both clear, default mode
 */
void nd500_instr_Ifstgo(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
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

    // Test if specified bit in status register (ST1) IS set
    bool bit_is_set = (cpu->ST1 & (1U << bit_number)) != 0;

    if (bit_is_set) {
        // Read displacement value and sign-extend based on data type
        uint64_t value = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);
        int64_t displacement = nd500_sign_extend_by_dtype(value, fi->data_type);

        // Update PC (relative branch from instruction start)
        cpu->PC = (uint32_t)(fi->address + displacement);

        // Set Branch Trap (BT) bit in status register when branch is taken
        cpu->ST1 |= 0x40000;  // BT bit = bit 18
    }
    // else: bit is not set, branch not taken, PC already points to next instruction

    // No status flags are modified by this instruction
}
