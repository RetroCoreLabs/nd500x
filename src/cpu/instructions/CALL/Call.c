#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Call instruction - CALL class
 *
 * Call subroutine with arguments. Sets up pending call state that is consumed
 * by the subsequent ENT* instruction at the subroutine entry point.
 *
 * Mnemonic: CALL
 * Operands: 2+ (subroutine address, argument count, ...argument operands)
 * Opcode: 0x00C3
 *
 * Format:
 *   CALL address, n, arg1, arg2, ..., argn
 *
 * Operation:
 *   1. Read subroutine address (operand 0)
 *   2. Read argument count n (operand 1)
 *   3. Calculate effective addresses of all n arguments (operands 2..n+1)
 *   4. Store call info in CPU pending_call state
 *   5. Save return address (current PC) in L and pending state
 *   6. Jump to subroutine address
 *
 * The ENT* instruction at the subroutine entry point will consume the
 * pending_call state to initialize the stack frame.
 *
 * Traps:
 *   - IOS (Illegal Operand Specifier) if argument operands are not memory operands
 *   - ISE (Instruction Sequence Error) if target is not an entry point (optional check)
 *
 * Reference: RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Call.cs
 */
void nd500_instr_Call(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate minimum operand count (address + arg_count) */
    if (fi->operand_count < 2) {
        printf("[ERROR] CALL at PC=0x%08X: Expected at least 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read subroutine address (operand 0 - word) */
    uint32_t subroutine_addr = nd500_read_operand_word(cpu, &fi->operands[0]);

    /* Read argument count (operand 1 - byte) */
    uint8_t arg_count = nd500_read_operand_byte(cpu, &fi->operands[1]);

    /* Validate operand count matches (2 + arg_count) */
    if (fi->operand_count != (2 + arg_count)) {
        printf("[ERROR] CALL at PC=0x%08X: Expected %u operands (2 + %u args), got %u\n",
               fi->address, 2 + arg_count, arg_count, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Calculate effective addresses of all arguments */
    for (uint32_t i = 0; i < arg_count && i < 256; i++) {
        const Nd500OperandDecoded* arg_operand = &fi->operands[2 + i];

        /* CALL arguments MUST be memory operands (not constants or registers) */
        /* We pass the ADDRESS of the argument, not the VALUE */
        if (arg_operand->mode == ND500_ADDR_CONSTANT ||
            arg_operand->mode == ND500_ADDR_CONSTANT_SHORT) {
            printf("[TRAP] CALL at PC=0x%08X: Argument %u is constant, must be memory operand\n",
                   fi->address, i + 1);
            trap_illegal_operand(cpu, fi->address);
            return;
        }

        /* Store effective address of this argument */
        cpu->pending_call_arg_addresses[i] = arg_operand->effective_address;
    }

    /* Store call information in CPU-internal state */
    cpu->pending_call_arg_count = arg_count;
    cpu->pending_call_return_address = cpu->PC;  /* Current PC (after this instruction) */

    /* Save return address in L register as well */
    cpu->L = cpu->PC;

    /* TODO (optional): Validate target address is an entry point instruction
     * Valid entry opcodes: ENTD (0x9C), ENTS (0xB8), ENTF (0xDE), etc.
     * For now, we skip this validation for simplicity.
     */

    /* TODO (Phase 4): Check for indirect segment call and domain switching
     * This requires domain system integration (deferred).
     */

    /* Jump to subroutine */
    cpu->PC = subroutine_addr;

    /* Call trap (CT) would be checked by trap system if enabled */
    /* Note: This is an ignorable trap that doesn't stop execution */
}
