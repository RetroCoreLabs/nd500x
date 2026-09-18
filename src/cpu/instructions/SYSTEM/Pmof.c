#include "cpu_protos.h"
#include "machine_protos.h"
#include "nd500_mmu.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * PMOF instruction - SYSTEM class
 *
 * Mnemonic: pmof
 * Operands: 0
 * Opcode: 0xFF19 (177431 octal)
 *
 * Operation: Turn off program memory management system; L -> P
 *
 * Description:
 * Privileged instruction.
 * Following instruction accesses will be interpreted directly as physical addresses,
 * rather than being mapped on a physical segment through the memory management system.
 * The physical address of the next instruction to be executed is found in the L register.
 * If the program memory management system is already turned off, control is transferred
 * to the physical address specified by the L register and the instruction has no further effect.
 *
 * Trap conditions: Illegal instruction code (IIC) if not privileged
 * Data status bits: Unaffected
 *
 * Reference: ND-500 Reference Manual ND-05.009.4 EN, Page 304, Section 16.16
 */
void nd500_instr_Pmof(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    (void)fi;  /* Unused parameter */

    /* Check privilege - PMOF requires supervisor mode */
    if (!nd500_require_privilege(cpu, fi->address)) {
        return;  /* Trapped - not privileged */
    }

    /* Disable program MMU (instruction fetches will now use physical addresses) */
    nd500_mmu_disable_program(cpu);

    /* Transfer control to physical address in L register */
    /* The next instruction fetch will NOT be translated (direct physical access) */
    cpu->PC = cpu->L;
}
