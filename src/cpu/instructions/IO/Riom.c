#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * RIOM instruction - IO class
 *
 * Mnemonic: riom
 * Operands: 3
 * Opcode: 0xFE76 (hex) / 0177166 (octal) / 65142 (decimal)
 *
 * Operation: Read I/O Processor Memory (ND-100 → ND-500 transfer)
 *
 * Description:
 * Privileged instruction that copies data from the I/O processor (ND-100) memory
 * to ND-500 memory through the ND-500 interface. This allows the ND-500 to access
 * private ND-100 memory that is not directly addressable by the ND-500.
 *
 * The ND-100 memory is accessed via DMA and does not interrupt ND-100 program
 * execution, allowing efficient data transfer between the two processors.
 *
 * Dual-Processor Architecture:
 * ND-500 systems use a companion I/O processor (ND-100) that handles peripheral
 * I/O operations while the ND-500 performs computation. RIOM enables inter-processor
 * communication by providing access to the ND-100's private memory space.
 *
 *   ┌──────────┐                    ┌──────────┐
 *   │  ND-500  │ ←──── RIOM ─────  │  ND-100  │
 *   │   CPU    │  (Read from IOP)   │   IOP    │
 *   └──────────┘                    └──────────┘
 *      Main Processor              I/O Processor
 *
 * Address Spaces:
 * - ND-500 uses 32-bit byte addressing (4GB address space)
 * - ND-100 uses 16-bit word addressing (128KB address space)
 * - Hardware bridge translates between address spaces
 * - _private offset (typically 0x40000) maps ND-100 space into physical RAM
 *
 * Transfer Mechanism:
 * 1. ND-500 issues RIOM instruction with ND-100 source address
 * 2. Hardware DMA controller reads from ND-100 memory space
 * 3. Data is transferred directly to ND-500 memory (no ND-100 interruption)
 * 4. ND-500 continues execution after transfer completes
 * 5. Efficient bulk transfer without CPU intervention
 *
 * Operand Structure:
 * - Operand[0]: ND-100 source address (read, word)
 *   - Source address in I/O processor memory (ND-100 word address)
 *   - Usually private ND-100 memory, not directly ND-500 addressable
 *   - Addressing modes: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
 *   - Data type: Word (physical ND-100 address)
 *
 * - Operand[1]: ND-500 destination buffer (write, halfword)
 *   - Destination buffer in ND-500 memory (logical address)
 *   - Addressing modes: LOCAL, RECORD, REGISTER, PRE_INDEXED, ABSOLUTE
 *   - Data type: Same as instruction prefix (halfword with H prefix)
 *
 * - Operand[2]: Count (read, halfword)
 *   - Number of halfwords to transfer (0-65535)
 *   - Addressing modes: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
 *   - Data type: Same as instruction prefix (halfword with H prefix)
 *
 * Operation Steps:
 * 1. Validate operand count (must be 3)
 * 2. Check supervisor mode (PIA bit must be set)
 * 3. Read ND-100 source address from operand[0] (word)
 * 4. Read ND-500 destination address from operand[1] (word)
 * 5. Read transfer count from operand[2] (halfword)
 * 6. Validate count (must fit in 16 bits)
 * 7. Validate ND-100 address (must fit in 16 bits)
 * 8. For i = 0 to count-1:
 *    a. Calculate ND-100 address = nd100_addr + i (word address, wraps at 16-bit)
 *    b. Calculate ND-500 address = nd500_addr + i×2 (byte address)
 *    c. Read halfword from ND-100 memory[nd100_addr]
 *    d. Write halfword to ND-500 memory[nd500_addr]
 * 9. Update Z flag based on count
 *
 * Memory Access Pattern:
 * - ND-100 source: Word address (multiply by 2 for byte offset)
 * - ND-500 dest: Byte address (increment by 2 for each halfword)
 * - Hardware DMA controller handles address translation and endianness
 *
 * Flag Behavior:
 * - Z (Zero): Set if count == 0 (no data transferred), cleared otherwise
 * - S, C, O, K: Unaffected
 *
 * Trap Conditions:
 * - Addressing traps: Invalid address, page fault, protection violation
 * - Illegal instruction code (IIC): Not in supervisor mode (requires PIA bit set)
 * - Illegal operand value (IOV): Invalid count or address
 *
 * Performance:
 * - Execution: Variable, depends on transfer size
 * - Overhead: ~10 cycles
 * - Transfer: ~2 cycles per halfword
 * - Total: ~(10 + 2×count) cycles
 * - DMA access does not interrupt ND-100 execution
 *
 * Key Characteristics:
 * - Supervisor-only DMA transfer from I/O processor to ND-500 memory
 * - Accesses ND-100 private memory not directly addressable by ND-500
 * - Halfword (16-bit) transfers only
 * - IIC trap if not in supervisor mode
 * - IOV trap on invalid address or count
 * - DMA does not interrupt ND-100 execution
 * - Essential for inter-processor communication
 *
 * Common Use Cases:
 * - Accessing I/O processor private memory
 * - Inter-processor data transfer
 * - Reading I/O processor status/configuration
 * - Debugging I/O processor state
 * - Shared memory communication between processors
 * - Reading device controller buffers via ND-100
 *
 * Typical Usage:
 *   Example 1: Copy one page from ND-100 memory
 *     H RIOM 66000B:W, PG, 1024        ; Copy 1024 halfwords
 *
 *   Example 2: Read I/O processor status
 *     H RIOM IOP_STATUS_ADDR:W, STATUS_BUFFER, 16
 *
 *   Example 3: Variable-sized transfer
 *     H RIOM SOURCE_ADDR, DEST_BUFFER, B.TRANSFER_SIZE
 *
 *   Example 4: Read configuration from ND-100
 *     H RIOM IOP_CONFIG:W, LOCAL_BUF, CONFIG_SIZE
 *
 * Notes:
 * - RIOM must be preceded by H prefix (halfword operations)
 * - Transfers are always in halfword (16-bit) units
 * - ND-100 address space wraps at 16-bit boundary (0x0000-0xFFFF)
 * - ND-500 address space is full 32-bit
 * - Requires supervisor mode (PIA bit set in status register)
 * - Use for inter-processor communication in dual-processor ND-500 systems
 *
 * IMPLEMENTATION STATUS: FUNCTIONAL (Basic bridge implemented)
 *
 * Current implementation:
 * - ND-100 bridge with address translation (nd100_memory_offset = 0x40000)
 * - Reads from shared physical memory at offset 0x40000
 * - Full transfer loop with proper address calculation
 * - Flag updates and validation
 *
 * Future enhancements:
 * 1. DMA controller simulation with cycle counting
 * 2. Endianness conversion (ND-100 is big-endian)
 * 3. Supervisor mode validation (PIA bit checking)
 *
 * Related Instructions:
 * - RIOM: Read I/O processor memory (ND-100 → ND-500) [this instruction]
 *
 * Reference: ND-500 Reference Manual, Section 16.23
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/IO/Riom.cs
 */
void nd500_instr_Riom(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 3) {
        printf("[ERROR] RIOM expects 3 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check supervisor mode - RIOM is a privileged instruction */
    /* PIA (Privileged Instruction Allowed) bit must be set */
    /* TODO: Implement PIA bit checking when status register is fully implemented */
    /* For now, we allow the instruction to execute */

    /* Read operands using nd500_read_operand_value for proper addressing mode support */
    /* Operand 0: ND-100 source address (word address in I/O processor space) */
    uint32_t nd100_source_addr = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Operand 1: ND-500 destination address (byte address in main processor space) */
    uint32_t nd500_dest_addr = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

    /* Operand 2: Count of halfwords to transfer */
    uint32_t count = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[2], fi->data_type);

    /* Validate count - must fit in 16 bits (halfword range 0-65535) */
    if (count > 0xFFFF) {
        printf("[ERROR] RIOM invalid count %u at PC=0x%08X\n", count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Validate ND-100 address - must fit in 22 bits (ND-100 physical address space)
     * ND-100 physical addressing: 24-bit word addresses (22-bit physical: 0x000000-0x3FFFFF)
     * Reference: ND100Bridge.md line 130 */
    if (nd100_source_addr > 0x3FFFFF) {
        printf("[ERROR] RIOM invalid ND-100 address 0x%08X at PC=0x%08X (max 0x3FFFFF)\n",
               nd100_source_addr, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check for potential address wraparound in ND-100 space */
    /* ND-100 has 22-bit physical address space (0x000000-0x3FFFFF) */
    if (nd100_source_addr + count > 0x400000) {
        printf("[WARNING] RIOM transfer wraps ND-100 address space at PC=0x%08X\n",
               fi->address);
        /* This is allowed but logged for debugging */
    }

    printf("[RIOM] Transfer: ND-100[0x%06X] → ND-500[0x%08X], count=%u halfwords at PC=0x%08X\n",
           nd100_source_addr, nd500_dest_addr, count, fi->address);

    /* ========================================================================
     * DMA TRANSFER FROM ND-100 TO ND-500
     * ========================================================================
     *
     * Transfer loop: Copy count halfwords from ND-100 memory to ND-500 memory
     *
     * Address spaces:
     * - ND-100 source: Word address (multiply by 2 for byte offset)
     * - ND-500 dest: Byte address (increment by 2 for each halfword)
     *
     * Hardware DMA controller handles:
     * - Address translation via _private offset
     * - Endianness conversion (ND-100 is big-endian)
     * - Concurrent ND-100 execution (no interruption)
     *
     * SIMULATED implementation:
     * - Writes zeros to destination (no actual ND-100 memory to read from)
     * - Real implementation would use nd100_bridge functions
     * ======================================================================== */

    for (uint32_t i = 0; i < count; i++) {
        /* Calculate addresses for this transfer iteration */
        /* ND-100 address: word-based, wraps at 22-bit boundary (0x000000-0x3FFFFF) */
        uint32_t nd100_addr = (nd100_source_addr + i) & 0x3FFFFF;

        /* ND-500 address: byte-based, each halfword is 2 bytes */
        uint32_t nd500_addr = nd500_dest_addr + (i * 2);

        /* Read halfword from ND-100 memory via bridge
         * The bridge handles:
         *   - Address translation (nd100_memory_offset + nd100_addr × 2)
         *   - Physical memory access to emulated ND-100 space
         *   - Big-endian byte order (both ND-100 and ND-500 use big-endian)
         */
        uint16_t data = nd500_read_nd100_word(cpu, nd100_addr);

        /* Write halfword to ND-500 memory */
        nd500_write_memory_16(cpu, nd500_addr, data);

        /* Debug trace for first and last transfers (avoid log spam for large transfers) */
        if (i == 0 || i == count - 1 || count <= 4) {
            printf("  RIOM[%u]: ND-100[0x%06X] = 0x%04X → ND-500[0x%08X]\n",
                   i, nd100_addr, data, nd500_addr);
        }
    }

    /* Update status flags per ND-500 Reference Manual §16.23 */
    /* Z (Zero): Set if count == 0 (no data transferred) */
    /* All other flags (S, C, O, K) unaffected */
    if (count == 0) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    printf("[RIOM] Completed: %u halfwords transferred, Z=%d at PC=0x%08X\n",
           count, (count == 0), fi->address);

    /* NOTE: In real hardware, this would be a DMA operation:
     * - ND-500 blocks until transfer completes (~10 + 2×count cycles)
     * - ND-100 continues executing (no interruption)
     * - Hardware DMA controller manages the transfer
     *
     * In emulator:
     * - Transfer is immediate (no cycle counting)
     * - Both processors are in same address space
     * - Bridge would provide address translation
     */
}
