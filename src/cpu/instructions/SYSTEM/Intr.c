#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Intr instruction - SYSTEM class
 *
 * INTR - Read Interrupt Request Register
 *
 * Format: tn INTR <dest/w/t>
 *
 * Assembly:
 *   Hn INTR (read 16-bit interrupt request)     Hex 0xFE68+(n-1)
 *   Wn INTR (read 32-bit interrupt request)     Hex 0xFE6C+(n-1)
 *
 * Operation: interrupt request register -> Rn
 *
 * Description:
 *   Privileged instruction that reads the interrupt request register
 *   into the specified integer register. This allows the operating
 *   system to query pending interrupts for interrupt handling.
 *
 *   EMULATOR NOTE: In the emulator, the interrupt request register
 *   is typically 0 (no pending interrupts) unless explicitly set
 *   by device emulation.
 *
 * Trap conditions: Illegal instruction code (IIC) if not privileged
 *
 * Data status bits: Z (result = 0), S (sign of result)
 *
 * Reference: ND-500 Reference Manual, Chapter 16.18
 */
void nd500_instr_Intr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] INTR at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Validate target register */
    if (fi->target_register < 1 || fi->target_register > 4) {
        printf("[ERROR] INTR at PC=0x%08X: Invalid target register %u\n",
               fi->address, fi->target_register);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check privileged mode - INTR is a privileged instruction */
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* Trap raised, instruction aborted */
    }

    /* Read interrupt request register */
    /* EMULATOR: Return 0 (no pending interrupts) */
    /* Full implementation would read from machine interrupt controller */
    uint32_t irr = 0;

    /* Write to target register */
    nd500_write_integer_register(cpu, fi->target_register, irr);

    /* Set status flags */
    if (irr == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    if (irr & 0x80000000) {
        cpu->ST1 |= ND500_FLAG_S;
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;
    }
}
