#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * ENTF instruction - CALL class
 *
 * Mnemonic: entf
 * Operands: 1
 * Opcode: 0x00DD
 *
 * Operation: Enter subroutine with fixed (static) data area
 *
 * Description:
 * Enters a subroutine using a pre-allocated fixed data area at a specified address.
 * Unlike ENTS (which allocates space on the stack), ENTF uses a static data area
 * whose address is provided as an operand. Variables in this area keep their values
 * between calls, making it suitable for functions that need persistent state.
 *
 * The data area address is a direct operand (4 bytes).
 *
 * Use ENTF for:
 * - Reentrant library functions with persistent state
 * - Functions that need static local variables
 * - Performance-critical functions (no allocation overhead)
 *
 * Operand Structure:
 * - Operand[0]: Fixed data area address (read, word)
 *
 * Operation Steps:
 * 1. Validate operand count (must be 1)
 * 2. Validate instruction sequence (must follow CALL)
 * 3. Read fixed data area address from operand
 * 4. Save old B register value
 * 5. Set B to fixed data area (NOT stack!)
 * 6. Initialize stack frame header in fixed area:
 *    a. B+0: PREVB = old B
 *    b. B+4: RETA = return address (from CALL)
 *    c. B+8: SP = old B's SP (inherit caller's stack pointer)
 *    d. B+12: AUX = 0
 *    e. B+16: N = argument count (from CALL)
 *    f. B+20+: Argument addresses (from CALL)
 * 7. Update L register to return address
 * 8. Update B register to fixed area
 * 9. Clear pending call state
 *
 * Stack Frame Structure (at fixed area):
 * - B+0: PREVB (previous B register value)
 * - B+4: RETA (return address)
 * - B+8: SP (stack pointer - inherited from caller)
 * - B+12: AUX (auxiliary field - always 0)
 * - B+16: N (argument count)
 * - B+20: ARG1 (first argument address)
 * - B+24: ARG2 (second argument address)
 * - ... (additional arguments)
 *
 * Note: B.SP is inherited from caller (oldB.SP), not modified.
 * This allows the subroutine to use caller's stack for temporaries.
 *
 * Flag Behavior:
 * - All flags unaffected
 *
 * Trap Conditions:
 * - Instruction sequence error (ISE) if not preceded by CALL
 * - Addressing traps if operand address is invalid
 *
 * Typical Usage:
 *   ; Allocate fixed data area
 *   COUNTER_DATA: .SPACE 100H
 *
 *   ; Call counter function
 *   CALL INCREMENT_COUNTER, 0
 *
 *   INCREMENT_COUNTER:
 *   ENTF COUNTER_DATA          ; Use fixed area
 *   W1 MOVE B.COUNT, I1        ; Access persistent variable
 *   W1 ADD $1                  ; Increment
 *   W1 MOVE B.COUNT            ; Store back
 *   RET
 *
 * Notes:
 * - ENTF must be preceded by CALL instruction
 * - Fixed area persists between calls (static storage)
 * - No allocation/deallocation overhead
 * - Suitable for library functions with persistent state
 * - Return using RET or RETK (not RETB/RETBK)
 *
 * Related Instructions:
 * - CALL: Call subroutine with arguments
 * - ENTF: Enter with fixed data area [this instruction]
 * - ENTFN: Enter with fixed area and limited arguments
 * - ENTS: Enter with stack-allocated data area
 * - ENTSN: Enter with stack-allocated area and limited arguments
 * - RET: Return from subroutine (clear K)
 * - RETK: Return from subroutine (set K)
 *
 * Reference: ND-500 Reference Manual, Chapter 13.10
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Entf.cs
 */
void nd500_instr_Entf(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Stack frame field offsets (predefined by architecture) */
    const uint32_t OFFSET_PREVB = 0;      /* Previous B */
    const uint32_t OFFSET_RETA  = 4;      /* Return address */
    const uint32_t OFFSET_SP    = 8;      /* Stack pointer */
    const uint32_t OFFSET_AUX   = 12;     /* Auxiliary field */
    const uint32_t OFFSET_N     = 16;     /* Argument count */
    const uint32_t OFFSET_ARG1  = 20;     /* First argument address */

    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] ENTF expects 1 operand, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Validate instruction sequence (must follow CALL) */
    if (cpu->pending_call_return_address == 0) {
        printf("[ERROR] ENTF at PC=0x%08X: Instruction sequence error - not preceded by CALL\n",
               fi->address);
        trap_instruction_sequence_error(cpu, fi->address);
        return;
    }

    /* Read fixed data area address (direct operand) */
    uint32_t data_area_addr = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Save old B */
    uint32_t old_b = cpu->B;

    /* STEP 1: Set B to fixed data area (NOT stack!) */
    uint32_t new_b = data_area_addr;

    /* STEP 2: Initialize stack frame header */
    nd500_write_memory_32(cpu, new_b + OFFSET_PREVB, old_b);

    uint32_t return_addr = cpu->pending_call_return_address;
    nd500_write_memory_32(cpu, new_b + OFFSET_RETA, return_addr);

    /* STEP 3: Copy SP from old frame (inherit caller's stack pointer) */
    uint32_t old_sp = nd500_read_memory_32(cpu, old_b + OFFSET_SP);

    /* A fault on that read leaves the value garbage; abort before it is used. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }
    nd500_write_memory_32(cpu, new_b + OFFSET_SP, old_sp);

    /* STEP 4: Initialize AUX */
    nd500_write_memory_32(cpu, new_b + OFFSET_AUX, 0);

    /* STEP 5: Write argument count */
    uint32_t arg_count = cpu->pending_call_arg_count;
    nd500_write_memory_32(cpu, new_b + OFFSET_N, arg_count);

    /* STEP 6: Copy argument addresses */
    for (uint32_t i = 0; i < arg_count && i < ND500_MAX_OPERANDS; i++) {
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

    printf("[ENTF] Using fixed data area at B=0x%08X, args=%u, ret=0x%08X\n",
           new_b, arg_count, return_addr);

    /* Data status bits are unaffected */
}
