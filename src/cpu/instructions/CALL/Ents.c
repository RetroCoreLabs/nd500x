#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include "nd500_mmu.h"
#include <stdio.h>
#include <stdlib.h>

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
        ND500X_TRAPLOG("[TRAP] ENTS at PC=0x%08X: Must be preceded by CALL/CALLG\n",
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
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Read old B.SP to get new B */
    uint32_t old_b = cpu->B;
    uint32_t new_b = nd500_read_memory_32(cpu, old_b + OFFSET_SP);

    /* A fault on that read leaves new_b garbage; abort before it is used. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Frame trace (env-gated) - shows where new_b comes from */
    if (getenv("ND500X_FRAMELOG")) {
        printf("[ENTS] PC=0x%08X old_b=0x%08X read[old_b+8=0x%08X]=new_b=0x%08X L=0x%08X\n",
               fi->address, old_b, old_b + OFFSET_SP, new_b, cpu->L);
    }

    /* Check for stack overflow BEFORE modifying anything */
    if (new_b + stack_demand >= cpu->TOS) {
        ND500X_TRAPLOG("[TRAP] ENTS at PC=0x%08X: Stack overflow (newB=0x%08X, demand=0x%08X, TOS=0x%08X)\n",
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

    /* A memory fault on ANY of the frame writes above must abort the instruction
     * BEFORE the commit below - it must not load L and must not release the
     * CALL/ENT* sequence interlock.
     *
     * MICRO-5800-B30 puts both of those in the TERMINAL microword, together with
     * the final WRITE and the exit to the next instruction:
     *   ENTS_END  @004206  ... D,DAC,REG05 ... WRITE ... ADDR=GET_NEXT
     *   ENTSN_3   @004254  ... C,SEQ ... F,RETURN INVSEQ ... WRITE ...
     * The earlier frame writes are separate microwords (ENTS_N1 @004176,
     * ENTS_3 @004216), so a fault on one of those traps out before the machine
     * ever loads L or asserts INVSEQ, and the retried ENTS still sees its CALL.
     *
     * Running on regardless is what killed vi: the ENTS at 0x0000FC15 faulted on
     * a frame write, fell through to set L = 0x0000F9EE and clear the interlock,
     * and the retry after pagein raised a false ISE ("Memory fault - core
     * dumped"). Same defect class as the Call.c entry-fetch guard. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Update B register to new stack frame */
    cpu->B = new_b;

    /* STKDBG: report each time the stack reaches a NEW high-water mark. If frames
     * were properly reused (alloc/dealloc balanced), the HWM would settle; a HWM
     * that keeps climbing pinpoints the call sites whose frames leak. Env-gated. */
    {
        static uint32_t hwm = 0;
        static int stkdbg = -1;
        if (stkdbg < 0) { const char* e = getenv("ND500X_STKDBG"); stkdbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
        if (stkdbg && new_b > hwm) {
            hwm = new_b;
            fprintf(stderr, "[STKDBG] stack HWM new_b=0x%08X depth=0x%X at PC=0x%08X (old_b=0x%08X)\n",
                    new_b, new_b - 0xE8000F00u, fi->address, old_b);
        }
    }

    /* Update L register with return address */
    cpu->L = cpu->pending_call_return_address;

    /* Clear pending call state */
    cpu->pending_call_return_address = 0;
    cpu->pending_call_arg_count = 0;

    /* STO status bit is set/reset for each ENTS, ENTSN, ENTB, INIT, ENTM and
     * GETB (ND-500 Reference Manual, traps section). Successful completion
     * resets it - the bit must not stay stale after an earlier overflow. */
    cpu->ST1 &= ~(uint32_t)TRAP_STO;

    /* PC already advanced by cpu_step() */
}
