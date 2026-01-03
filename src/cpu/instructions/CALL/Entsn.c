#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Entsn instruction - CALL class
 *
 * Enter Stack Subroutine with Maximum Arguments - Like ENTS but limits
 * the number of arguments transferred to the stack frame.
 *
 * Mnemonic: ENTSN
 * Operands: 2 (stack demand in BYTES - per ND-05.009.4 Section 13.10, max argument count)
 * Opcode: 0x00BA
 *
 * Format: ENTSN <stack_demand>, <max_args>
 *
 * Stack Frame Layout (same as ENTS):
 *   +0   PREVB    Previous B register value
 *   +4   RETA     Return address
 *   +8   SP       Stack pointer (B + stack_demand)
 *   +12  AUX      Auxiliary field (0 for ENTSN)
 *   +16  N        Argument count (LIMITED to max_args!)
 *   +20  ARG1     First argument address
 *   +24  ARG2     Second argument address
 *   ...  ...      (up to max_args arguments)
 *
 * Operation:
 *   Same as ENTS, but B.N = min(actual_args, max_args)
 *   Only the first max_args arguments are copied to the stack frame.
 *
 * Use cases:
 *   - Variable-argument functions (e.g., printf)
 *   - Functions with optional parameters
 *   - Protecting stack from excessive argument counts
 *
 * Example:
 *   CALL PRINTF, 5, FMT, A1, A2, A3, A4    ; 5 args provided
 *   PRINTF:
 *     ENTSN 200H, 3                        ; Accept max 3
 *     ; B.N = 3 (not 5!)
 *     ; Only FMT, A1, A2 are in stack frame
 *
 * Traps:
 *   - ISE (Instruction Sequence Error) if CALL did not precede
 *   - STO (Stack Overflow) if stack demand exceeds available space
 *
 * Reference: RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Entsn.cs
 */
void nd500_instr_Entsn(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Stack frame field offsets */
    const uint32_t OFFSET_PREVB = 0;   /* Previous B */
    const uint32_t OFFSET_RETA  = 4;   /* Return address */
    const uint32_t OFFSET_SP    = 8;   /* Stack pointer */
    const uint32_t OFFSET_AUX   = 12;  /* Auxiliary */
    const uint32_t OFFSET_N     = 16;  /* Argument count */
    const uint32_t OFFSET_ARG1  = 20;  /* First argument address */

    /* Validate that CALL preceded this instruction */
    if (cpu->pending_call_return_address == 0) {
        printf("[TRAP] ENTSN at PC=0x%08X: Must be preceded by CALL/CALLG\n",
               fi->address);
        trap_instruction_sequence_error(cpu, fi->address);
        return;
    }

    /* Validate operand count */
    if (fi->operand_count != 2) {
        printf("[ERROR] ENTSN at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read stack demand operand (already in bytes per ND-05.009.4 Section 13.10) */
    uint32_t stack_demand = nd500_read_operand_word(cpu, &fi->operands[0]);

    /* Read maximum argument count (operand 1) */
    uint32_t max_args = nd500_read_operand_word(cpu, &fi->operands[1]);

    /* Read old B.SP to get new B */
    uint32_t old_b = cpu->B;
    uint32_t new_b = nd500_read_memory_32(cpu, old_b + OFFSET_SP);

    /* Check for stack overflow BEFORE modifying anything */
    if (new_b + stack_demand >= cpu->TOS) {
        printf("[TRAP] ENTSN at PC=0x%08X: Stack overflow (newB=0x%08X, demand=0x%08X, TOS=0x%08X)\n",
               fi->address, new_b, stack_demand, cpu->TOS);
        trap_stack_overflow(cpu, fi->address);
        return;
    }

    /* Limit argument count to maximum */
    uint32_t actual_arg_count = cpu->pending_call_arg_count;
    uint32_t transfer_count = (actual_arg_count < max_args) ? actual_arg_count : max_args;

    /* Initialize new stack frame */
    /* B.PREVB = old B */
    nd500_write_memory_32(cpu, new_b + OFFSET_PREVB, old_b);

    /* B.RETA = return address */
    nd500_write_memory_32(cpu, new_b + OFFSET_RETA, cpu->pending_call_return_address);

    /* B.SP = new B + stack demand */
    nd500_write_memory_32(cpu, new_b + OFFSET_SP, new_b + stack_demand);

    /* B.AUX = 0 (not used for ENTSN) */
    nd500_write_memory_32(cpu, new_b + OFFSET_AUX, 0);

    /* B.N = ACTUAL transferred count (not caller's count!) */
    nd500_write_memory_32(cpu, new_b + OFFSET_N, transfer_count);

    /* Copy only the first max_args argument addresses */
    for (uint32_t i = 0; i < transfer_count && i < ND500_MAX_OPERANDS; i++) {
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
