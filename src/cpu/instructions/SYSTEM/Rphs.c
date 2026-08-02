#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Rphs instruction - SYSTEM class
 *
 * RPHS - Read from Physical Segment
 *
 * Format: RPHS <logical address/r/W>, <physical segment/r/W>, <number of bytes/r/W>
 *
 * Assembly:
 *   RPHS (read from physical segment)         Hex 0xFFF5
 *
 * Operation: Copy number of bytes from logical address on physical segment
 *            to logical address on the domain.
 *
 * Description:
 *   Privileged instruction that copies a specified number of bytes from
 *   a logical address on a physical segment to a logical address on the
 *   current domain. This is used for direct memory access operations and
 *   system-level memory management.
 *
 *   Uses I1 for byte count and I2 for destination pointer.
 *   After completion, I1 = 0 (all bytes copied) and I2 points past
 *   the last byte written.
 *
 *   EMULATOR NOTE: This is a basic implementation that performs a
 *   memory-to-memory copy. Physical segment mapping is not fully
 *   implemented.
 *
 * Trap conditions: Addressing traps, Illegal instruction code (IIC)
 *
 * Data status bits: Z = 1 (operation complete)
 *
 * Reference: ND-500 Reference Manual, Chapter 16.31
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Rphs.cs
 */
void nd500_instr_Rphs(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 42-46) */
    if (fi->operand_count != 3) {
        printf("[ERROR] RPHS at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check privileged mode - RPHS requires privileged access (like C# lines 49-53) */
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* Trap raised, instruction aborted */
    }

    /* Read parameters (like C# lines 57-60) */
    uint32_t source_address = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }
    /* uint32_t physical_segment = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_WORD); */
    uint32_t byte_count = cpu->I[0];   /* I1 = byte count */
    uint32_t dest_address = cpu->I[1]; /* I2 = destination pointer */

    /* BASIC IMPLEMENTATION: Copy bytes from source to destination */
    /* Full implementation would handle physical segment mapping */
    /* For emulator, we treat this as a memory-to-memory copy */
    for (uint32_t i = 0; i < byte_count; i++) {
        uint8_t value = nd500_read_memory_8(cpu, source_address + i);
        nd500_write_memory_8(cpu, dest_address + i, value);

        /* Check for trap during memory access */
        if (nd500_trap_occurred()) {
            return;
        }
    }

    /* Update I1 (bytes remaining) and I2 (dest pointer) (like C# lines 73-74) */
    cpu->I[0] = 0;  /* All bytes copied */
    cpu->I[1] = dest_address + byte_count;

    /* Set Z flag - operation complete (like C# line 77) */
    cpu->ST1 |= ND500_FLAG_Z;
}
