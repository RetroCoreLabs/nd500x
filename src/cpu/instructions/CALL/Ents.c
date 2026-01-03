#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Ents instruction - CALL class
 *
 * Enter Stack Subroutine - Standard entry point with stack frame allocation.
 * Creates a new stack frame linked to the previous frame.
 *
 * Mnemonic: ENTS
 * Operands: 1 (stack demand in BYTES - per ND-05.009.4 Section 13.10)
 * Opcode: 0x00B8
 *
 * Stack Frame Layout (offsets from B):
 *   +0   PREVB    Previous B register value
 *   +4   RETA     Return address
 *   +8   SP       Stack pointer (B + stack_demand)
 *   +12  AUX      Auxiliary field (0 for ENTS)
 *   +16  N        Argument count
 *   +20  ARG1     First argument address
 *   +24  ARG2     Second argument address
 *   ...  ...      (additional arguments)
 *
 * Operation:
 *   1. Validate CALL preceded this
 *   2. Read stack_demand operand
 *   3. Read B.SP from old frame to get new B
 *   4. Check stack overflow (newB + stack_demand >= TOS)
 *   5. Initialize new stack frame
 *   6. Update B register
 *   7. Clear pending call state
 *
 * Traps:
 *   - ISE (Instruction Sequence Error) if CALL did not precede
 *   - STO (Stack Overflow) if stack demand exceeds available space
 *
 * Reference: RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Ents.cs
 */
void nd500_instr_Ents(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Stack frame field offsets */
    const uint32_t OFFSET_PREVB = 0;   /* Previous B */
    const uint32_t OFFSET_RETA  = 4;   /* Return address */
    const uint32_t OFFSET_SP    = 8;   /* Stack pointer */
    const uint32_t OFFSET_AUX   = 12;  /* Auxiliary */
    const uint32_t OFFSET_N     = 16;  /* Argument count */
    const uint32_t OFFSET_ARG1  = 20;  /* First argument address */

    /* Validate that CALL preceded this instruction */
    if (cpu->pending_call_return_address == 0) {
        printf("[TRAP] ENTS at PC=0x%08X: Must be preceded by CALL/CALLG\n",
               fi->address);
        trap_instruction_sequence_error(cpu, fi->address);
        return;
    }

    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] ENTS at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read stack demand operand (already in bytes per ND-05.009.4 Section 13.10) */
    uint32_t stack_demand = nd500_read_operand_word(cpu, &fi->operands[0]);

    /* Read old B.SP to get new B */
    uint32_t old_b = cpu->B;
    uint32_t new_b = nd500_read_memory_32(cpu, old_b + OFFSET_SP);

    /* Check for stack overflow BEFORE modifying anything */
    if (new_b + stack_demand >= cpu->TOS) {
        printf("[TRAP] ENTS at PC=0x%08X: Stack overflow (newB=0x%08X, demand=0x%08X, TOS=0x%08X)\n",
               fi->address, new_b, stack_demand, cpu->TOS);
        trap_stack_overflow(cpu, fi->address);
        return;
    }

    /* Initialize new stack frame */
    /* B.PREVB = old B */
    nd500_write_memory_32(cpu, new_b + OFFSET_PREVB, old_b);

    /* B.RETA = return address */
    nd500_write_memory_32(cpu, new_b + OFFSET_RETA, cpu->pending_call_return_address);

    /* B.SP = new B + stack demand */
    nd500_write_memory_32(cpu, new_b + OFFSET_SP, new_b + stack_demand);

    /* B.AUX = 0 (not used for ENTS) */
    nd500_write_memory_32(cpu, new_b + OFFSET_AUX, 0);

    /* B.N = argument count */
    nd500_write_memory_32(cpu, new_b + OFFSET_N, cpu->pending_call_arg_count);

    /* Copy argument addresses */
    for (uint32_t i = 0; i < cpu->pending_call_arg_count && i < ND500_MAX_OPERANDS; i++) {
        nd500_write_memory_32(cpu, new_b + OFFSET_ARG1 + (i * 4),
                             cpu->pending_call_arg_addresses[i]);
    }

    /* Update B register to new stack frame */
    cpu->B = new_b;

    /* Update L register with return address */
    cpu->L = cpu->pending_call_return_address;

    /* Clear pending call state */
    cpu->pending_call_return_address = 0;
    cpu->pending_call_arg_count = 0;

    /* PC already advanced by cpu_step() */
}
