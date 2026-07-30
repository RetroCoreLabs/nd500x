#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * ENTFN instruction - CALL class
 *
 * Mnemonic: entfn
 * Operands: 2
 * Opcode: 0x00DE
 *
 * Operation: Enter subroutine with fixed data area and maximum arguments
 *
 * Description:
 * Combines features of ENTF and ENTSN:
 * - Uses fixed (static) data area like ENTF
 * - Limits argument count like ENTSN
 *
 * Enters a subroutine using a pre-allocated fixed data area at a specified address,
 * but only the first <max no. of arg.> arguments are transferred to the stack frame.
 * Any remaining arguments from the CALL instruction are ignored.
 *
 * The data area address and maximum argument count are direct operands.
 *
 * Use ENTFN for:
 * - Library functions with static data and variable args
 * - Functions that accept optional parameters
 * - State machines with persistent data
 *
 * Operand Structure:
 * - Operand[0]: Fixed data area address (read, word)
 * - Operand[1]: Maximum number of arguments (read, word)
 *
 * Operation Steps:
 * 1. Validate operand count (must be 2)
 * 2. Validate instruction sequence (must follow CALL)
 * 3. Read fixed data area address and max args from operands
 * 4. Save old B register value
 * 5. Set B to fixed data area (NOT stack!)
 * 6. Initialize stack frame header in fixed area:
 *    a. B+0: PREVB = old B
 *    b. B+4: RETA = return address (from CALL)
 *    c. B+8: SP = old B's SP (inherit caller's stack pointer)
 *    d. B+12: AUX = 0
 *    e. B+16: N = min(actual_args, max_args)
 *    f. B+20+: First N argument addresses (from CALL)
 * 7. Update L register to return address
 * 8. Update B register to fixed area
 * 9. Clear pending call state
 *
 * Stack Frame Structure (at fixed area):
 * - B+0: PREVB (previous B register value)
 * - B+4: RETA (return address)
 * - B+8: SP (stack pointer - inherited from caller)
 * - B+12: AUX (auxiliary field - always 0)
 * - B+16: N (min(actual argument count, max arguments))
 * - B+20: ARG1 (first argument address)
 * - B+24: ARG2 (second argument address)
 * - ... (up to N arguments)
 *
 * Note: B.SP is inherited from caller (oldB.SP), not modified.
 * This allows the subroutine to use caller's stack for temporaries.
 *
 * Argument Limiting:
 * If CALL provides 10 arguments but max_args is 5, only the first 5 arguments
 * are transferred and B.N is set to 5 (not 10).
 *
 * Flag Behavior:
 * - All flags unaffected
 *
 * Trap Conditions:
 * - Instruction sequence error (ISE) if not preceded by CALL
 * - Addressing traps if operand addresses are invalid
 *
 * Typical Usage:
 *   LOGGER_DATA: .SPACE 200H
 *
 *   CALL LOG, 10, ...args...    ; Many arguments
 *
 *   LOG:
 *   ENTFN LOGGER_DATA, 5        ; Accept max 5
 *   ; B points to LOGGER_DATA (persistent)
 *   ; B.N = 5 (not 10)
 *   ; First 5 arguments in B.20+
 *   RET
 *
 * Notes:
 * - ENTFN must be preceded by CALL instruction
 * - Fixed area persists between calls (static storage)
 * - Combines fixed storage with argument limiting
 * - Suitable for library functions with variadic args
 * - Return using RET or RETK (not RETB/RETBK)
 *
 * Related Instructions:
 * - CALL: Call subroutine with arguments
 * - ENTF: Enter with fixed data area
 * - ENTFN: Enter with fixed area and limited arguments [this instruction]
 * - ENTS: Enter with stack-allocated data area
 * - ENTSN: Enter with stack-allocated area and limited arguments
 * - RET: Return from subroutine (clear K)
 * - RETK: Return from subroutine (set K)
 *
 * Reference: ND-500 Reference Manual, Chapter 13.10
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Entfn.cs
 */
void nd500_instr_Entfn(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Stack frame field offsets (predefined by architecture) */
    const uint32_t OFFSET_PREVB = 0;      /* Previous B */
    const uint32_t OFFSET_RETA  = 4;      /* Return address */
    const uint32_t OFFSET_SP    = 8;      /* Stack pointer */
    const uint32_t OFFSET_AUX   = 12;     /* Auxiliary field */
    const uint32_t OFFSET_N     = 16;     /* Argument count */
    const uint32_t OFFSET_ARG1  = 20;     /* First argument address */

    /* Validate operand count */
    if (fi->operand_count != 2) {
        printf("[ERROR] ENTFN expects 2 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Validate instruction sequence (must follow CALL) */
    if (cpu->pending_call_return_address == 0) {
        printf("[ERROR] ENTFN at PC=0x%08X: Instruction sequence error - not preceded by CALL\n",
               fi->address);
        trap_instruction_sequence_error(cpu, fi->address);
        return;
    }

    /* Read operands */
    uint32_t data_area_addr = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    uint32_t max_args = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

    /* Save old B */
    uint32_t old_b = cpu->B;

    /* STEP 1: Set B to fixed data area (NOT stack!) */
    uint32_t new_b = data_area_addr;

    /* STEP 2: Initialize stack frame header */
    nd500_write_memory_32(cpu, new_b + OFFSET_PREVB, old_b);

    uint32_t return_addr = cpu->pending_call_return_address;
    nd500_write_memory_32(cpu, new_b + OFFSET_RETA, return_addr);

    /* STEP 3: Inherit caller's SP (like ENTF) */
    uint32_t old_sp = nd500_read_memory_32(cpu, old_b + OFFSET_SP);

    /* A fault on that read leaves the value garbage; abort before it is used. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }
    nd500_write_memory_32(cpu, new_b + OFFSET_SP, old_sp);

    /* STEP 4: Initialize AUX */
    nd500_write_memory_32(cpu, new_b + OFFSET_AUX, 0);

    /* STEP 5: Limit argument count to maximum (like ENTSN) */
    uint32_t actual_arg_count = cpu->pending_call_arg_count;
    uint32_t transfer_count = (actual_arg_count < max_args) ? actual_arg_count : max_args;

    /* Write the ACTUAL transferred count */
    nd500_write_memory_32(cpu, new_b + OFFSET_N, transfer_count);

    /* STEP 6: Copy limited arguments */
    for (uint32_t i = 0; i < transfer_count; i++) {
        uint32_t arg_addr = cpu->pending_call_arg_addresses[i];
        nd500_write_memory_32(cpu, new_b + OFFSET_ARG1 + (i * 4), arg_addr);
    }

    /* STEP 7: Update B register to fixed area */

    /* A memory fault on any access above must abort BEFORE the commit below: the
     * real machine loads L and releases the CALL/ENT* sequence interlock only in
     * the TERMINAL microword (MICRO-5800-B30 ENTS_END @004206 loads L via
     * D,DAC,REG05; ENTSN_3 @004254 asserts C,SEQ / INVSEQ), both alongside the
     * final WRITE and the exit to the next instruction. Earlier frame writes are
     * separate microwords, so a fault there leaves L and the interlock untouched
     * and the retried entry instruction still sees its CALL. Without this the
     * retry raises a FALSE ISE - the defect that killed vi through ENTS. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    cpu->B = new_b;

    /* L is a terminal-microword effect - committed here, not at the RETA write. */
    cpu->L = return_addr;

    /* Clear pending call state */
    cpu->pending_call_return_address = 0;
    cpu->pending_call_arg_count = 0;

    printf("[ENTFN] Using fixed area at B=0x%08X, args=%u/%u (max=%u), ret=0x%08X\n",
           new_b, transfer_count, actual_arg_count, max_args, return_addr);

    /* Data status bits are unaffected */
}
