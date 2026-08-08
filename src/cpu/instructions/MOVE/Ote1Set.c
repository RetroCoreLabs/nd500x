#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Ote1Set instruction - MOVE class
 *
 * Load OTE1 Register: <source> → OTE1
 *
 * Variants: 1
 * Mnemonics: ote1:=
 * Operands: 1 (<source/r/t>)
 *
 * Opcode: 0xFDBB
 *
 * Operation: <source> → regs.OTE1
 *
 * Description:
 *   Reads the source operand as a word (32-bit) and stores it to the OTE1
 *   (Object Table Entry 1) register. Updates Z and S flags based on the value.
 *
 *   The OTE registers contain object table entry information used for
 *   object-oriented programming support and descriptor management.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if value is zero
 *   S = 1 if bit 31 is set (word sign bit)
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/Ote1Set.cs
 */
void nd500_instr_Ote1Set(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] Ote1Set at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint32_t value = nd500_read_operand_word(cpu, &fi->operands[0]);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* TEMM gating (manual OTE load, ref line 10253): a bit in OTE may only be
     * modified if the corresponding TEMM1 bit is set. Attempting to change a
     * non-modifiable bit causes an illegal operand value trap; OTE1 is left
     * unchanged. */
    uint32_t changed = value ^ cpu->OTE1;
    if (!nd500_temm_allows_change(changed, cpu->TEMM1)) {
        raise_trap(cpu, TRAP_IOV, fi->address, 0);
        return;
    }

    cpu->OTE1 = value;

    // Set Z and S flags based on the value
    if (value == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    if ((value & 0x80000000) != 0) {
        cpu->ST1 |= ND500_FLAG_S;
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;
    }
}
