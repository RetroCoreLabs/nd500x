#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Entd instruction - CALL class
 *
 * Enter Direct - Minimal entry point with no stack frame allocation.
 * This is the simplest entry point instruction, used for leaf functions
 * that don't need a stack frame.
 *
 * Mnemonic: ENTD
 * Operands: 0
 * Opcode: 0x009C
 *
 * Operation:
 *   1. Validate CALL preceded this instruction
 *   2. Validate zero arguments
 *   3. L ← return_address (save return address in L register)
 *   4. Clear pending call state
 *
 * No stack frame is created. The function uses the caller's stack frame.
 *
 * Traps:
 *   - ISE (Instruction Sequence Error) if CALL did not precede this
 *   - ISE if argument count is not zero
 *
 * Reference: RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Entd.cs
 */
void nd500_instr_Entd(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate that CALL preceded this instruction */
    if (cpu->pending_call_return_address == 0) {
        ND500X_TRAPLOG("[TRAP] ENTD at PC=0x%08X: Must be preceded by CALL/CALLG\n",
               fi->address);
        trap_instruction_sequence_error(cpu, fi->address);
        return;
    }

    /* Validate zero arguments (ENTD doesn't support arguments) */
    if (cpu->pending_call_arg_count != 0) {
        ND500X_TRAPLOG("[TRAP] ENTD at PC=0x%08X: Requires 0 arguments, got %u\n",
               fi->address, cpu->pending_call_arg_count);
        trap_instruction_sequence_error(cpu, fi->address);
        return;
    }

    /* Save return address in L register (only thing ENTD does) */
    cpu->L = cpu->pending_call_return_address;

    /* Clear pending call state */
    cpu->pending_call_return_address = 0;
    cpu->pending_call_arg_count = 0;

    /* No stack frame created - function uses caller's B register */
    /* PC already advanced by cpu_step() */
}
