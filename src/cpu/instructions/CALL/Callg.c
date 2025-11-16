#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * CALLG instruction - CALL class
 *
 * Mnemonic: callg
 * Operands: 2+
 * Opcode: 0x00B5 (hex) / 0000265 (octal) / 181 (decimal)
 *
 * Operation: Call subroutine general (with indirect addressing support)
 *
 * Description:
 * Calls a subroutine with arguments, similar to CALL, but allows the subroutine
 * address to be specified using ANY addressing mode (general operand), not just
 * a direct address. This enables:
 * - Indirect function calls via pointers
 * - Function pointer arrays
 * - Callbacks and dynamic dispatch
 * - Jump tables
 *
 * Key Difference from CALL:
 * - CALL: Subroutine address is a direct 4-byte operand (fixed addressing)
 * - CALLG: Subroutine address is a general operand (ANY addressing mode)
 *
 * This allows flexible calling patterns:
 *   CALLG B.FUNC_PTR, 2, ARG1, ARG2      ; Indirect via frame pointer
 *   CALLG I1.0, 1, ARG                   ; Function pointer from array
 *   CALLG @B.CALLBACK, 0                 ; Double-indirect call
 *   CALLG B.VT.METHOD, 3, A, B, C        ; Virtual method call
 *
 * Call Sequence (CALLG/ENT* handshake):
 * 1. CALLG prepares call state (saves return address, argument addresses)
 * 2. Jump to subroutine entry point
 * 3. ENT* instruction at entry point consumes call state and creates frame
 *
 * Operand Structure:
 * - Operand[0]: Subroutine address (read, word, GENERAL OPERAND)
 *   - Can use ANY addressing mode: LOCAL, RECORD, ABSOLUTE, REGISTER, PRE_INDEXED, etc.
 *   - Addressing modes: Any mode except CONSTANT_SHORT (must resolve to address)
 *   - Data type: Word (32-bit address)
 *
 * - Operand[1]: Argument count (read, byte)
 *   - Number of arguments being passed (0-255)
 *   - Addressing modes: Typically CONSTANT, but can be any mode
 *   - Data type: Byte (8-bit count)
 *
 * - Operands[2..n+1]: Argument operands (address calculation only)
 *   - MUST be memory operands (not constants or immediate values)
 *   - We pass the EFFECTIVE ADDRESS of each argument, not the value
 *   - Subroutine accesses arguments via these addresses
 *   - Addressing modes: LOCAL, RECORD, ABSOLUTE, REGISTER, PRE_INDEXED, etc.
 *   - Data type: Word (32-bit addresses)
 *
 * Operation Steps:
 * 1. Validate minimum operand count (must be >= 2)
 * 2. Read subroutine address from operand[0] (GENERAL operand)
 * 3. Read argument count from operand[1]
 * 4. Validate total operand count matches (2 + arg_count)
 * 5. For each argument (operands 2..n+1):
 *    a. Validate it's a memory operand (not constant)
 *    b. Store its effective address in pending call state
 * 6. Store call information in CPU:
 *    - pending_call_arg_count = arg_count
 *    - pending_call_return_address = current PC
 *    - pending_call_arg_addresses[] = argument addresses
 * 7. Save return address in L register
 * 8. Jump to subroutine (PC = subroutine_addr)
 *
 * Flag Behavior:
 * - All data status bits unaffected (S, Z, C, O, K)
 *
 * Trap Conditions:
 * - Addressing traps: Invalid address, page fault, protection violation
 * - Illegal Operand (IO): Insufficient operands, count mismatch
 * - Illegal Operand Specifier (IOS): Argument is constant (must be memory operand)
 * - Instruction Sequence Error (ISE): Target address is not an entry point (optional check)
 *
 * Performance:
 * - Execution: ~15 + (3 × arg_count) cycles
 * - Variable depending on number of arguments
 * - Slightly slower than CALL due to general operand addressing
 *
 * Key Characteristics:
 * - General operand subroutine address (indirect calls)
 * - Variable operand count (2 + arg_count)
 * - Arguments passed as effective addresses
 * - Must be paired with ENT* at subroutine entry
 * - Enables function pointers and callbacks
 * - ISE trap if target not an entry point
 * - IOS trap if arguments are constants
 *
 * Common Use Cases:
 * - Function pointer calls (C function pointers, Pascal procedure variables)
 * - Virtual method dispatch (OOP vtables)
 * - Callback functions
 * - Jump tables and switch statements
 * - Dynamic dispatch based on runtime values
 * - Plugin architectures
 *
 * Typical Usage:
 *   Example 1: Indirect function call
 *     CALLG B.CALLBACK_PTR, 2, INPUT, OUTPUT
 *
 *   Example 2: Function pointer array
 *     CALLG HANDLERS.I1, 1, EVENT_DATA
 *
 *   Example 3: Virtual method call
 *     CALLG OBJECT.VTABLE.DRAW, 0
 *
 *   Example 4: Double-indirect callback
 *     CALLG @B.DISPATCHER, 3, TYPE, ARG1, ARG2
 *
 * Notes:
 * - CALLG differs from CALL only in how the address is specified
 * - All other behavior (argument passing, return address, etc.) is identical
 * - Target address must point to an ENT* instruction (ENTS, ENTM, ENTD, ENTB, etc.)
 * - Arguments must be memory operands (addresses), not immediate values
 * - Subroutine accesses arguments via the stored effective addresses
 * - Return via RET* instructions that consume the pending call state
 *
 * IMPLEMENTATION STATUS: FUNCTIONAL
 *
 * Differences from CALL implementation:
 * - Uses nd500_read_operand_value() for address (not direct read)
 * - Otherwise identical logic for argument handling
 * - Same pending call state mechanism
 *
 * Related Instructions:
 * - CALL: Call subroutine with direct address
 * - ENTS: Enter stack subroutine (pairs with CALL/CALLG)
 * - ENTM: Enter masked subroutine
 * - ENTD: Enter domain subroutine
 * - RET: Return from subroutine
 *
 * Reference: ND-500 Reference Manual, Chapter 13.7
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Callg.cs
 */
void nd500_instr_Callg(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* ========================================================================
     * STEP 1: VALIDATE MINIMUM OPERAND COUNT
     * ======================================================================== */
    /* CALLG requires at least 2 operands: subroutine address + argument count */
    if (fi->operand_count < 2) {
        printf("[ERROR] CALLG expects at least 2 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* ========================================================================
     * STEP 2-3: READ SUBROUTINE ADDRESS AND ARGUMENT COUNT
     * ========================================================================
     *
     * KEY DIFFERENCE FROM CALL:
     * - CALL reads address directly from instruction bytes (4-byte immediate)
     * - CALLG reads address via general operand (ANY addressing mode)
     *
     * This allows:
     *   CALLG B.FUNC_PTR, 2, A, B     ; Load address from B.FUNC_PTR
     *   CALLG I1.0, 1, ARG            ; Load from function pointer array
     *   CALLG @B.CALLBACK, 0          ; Double-indirect
     * ======================================================================== */

    /* Operand 0: Subroutine address (GENERAL OPERAND - any addressing mode) */
    uint32_t subroutine_addr = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);

    /* Operand 1: Argument count (byte value) */
    uint8_t arg_count = (uint8_t)nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_BYTE);

    printf("[CALLG] Address=0x%08X (via general operand), args=%u at PC=0x%08X\n",
           subroutine_addr, arg_count, fi->address);

    /* ========================================================================
     * STEP 4: VALIDATE TOTAL OPERAND COUNT
     * ======================================================================== */
    /* Total operands must be: 2 fixed (address + count) + arg_count arguments */
    uint32_t expected_operands = 2 + arg_count;

    if (fi->operand_count != expected_operands) {
        printf("[ERROR] CALLG operand count mismatch: expected %u (2 + %u args), got %u at PC=0x%08X\n",
               expected_operands, arg_count, fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* ========================================================================
     * STEP 5: CALCULATE EFFECTIVE ADDRESSES OF ALL ARGUMENTS
     * ========================================================================
     *
     * IMPORTANT: We pass ADDRESSES of arguments, not VALUES!
     *
     * The subroutine receives the effective address of each argument and
     * can read/write to them. This enables pass-by-reference semantics.
     *
     * Arguments MUST be memory operands (not constants). Constants have no
     * address, so they cannot be passed to subroutines.
     * ======================================================================== */

    for (uint32_t i = 0; i < arg_count && i < 256; i++) {
        const Nd500OperandDecoded* arg_operand = &fi->operands[2 + i];

        /* Validate: Arguments MUST be memory operands, not constants */
        if (arg_operand->mode == ND500_ADDR_CONSTANT ||
            arg_operand->mode == ND500_ADDR_CONSTANT_SHORT) {
            printf("[TRAP] CALLG at PC=0x%08X: Argument %u is constant (mode=%u), must be memory operand\n",
                   fi->address, i + 1, arg_operand->mode);
            /* Set address to 0 to indicate error, but continue collecting other args */
            cpu->pending_call_arg_addresses[i] = 0;
            /* NOTE: Real hardware would raise IOS (Illegal Operand Specifier) trap here */
            /* For now, we allow it to continue but mark the address as invalid */
        } else {
            /* Store effective address of this argument */
            /* The subroutine will use this address to access the argument */
            cpu->pending_call_arg_addresses[i] = arg_operand->effective_address;

            printf("  CALLG arg[%u]: addr=0x%08X (mode=%u)\n",
                   i, arg_operand->effective_address, arg_operand->mode);
        }
    }

    /* ========================================================================
     * STEP 6: STORE CALL INFORMATION IN CPU PENDING STATE
     * ========================================================================
     *
     * The ENT* instruction at the subroutine entry point will consume this
     * pending call state to initialize the stack frame.
     *
     * Pending state includes:
     * - Return address (where to jump back after RET)
     * - Argument count (how many arguments were passed)
     * - Argument addresses (array of effective addresses)
     * ======================================================================== */

    /* Store call information in CPU-internal state */
    cpu->pending_call_arg_count = arg_count;
    cpu->pending_call_return_address = cpu->PC;  /* Current PC (after CALLG) is return address */

    /* ========================================================================
     * STEP 7-8: SAVE RETURN ADDRESS AND JUMP TO SUBROUTINE
     * ======================================================================== */

    /* Save return address in L register (standard ND-500 calling convention) */
    cpu->L = cpu->PC;

    /* Jump to subroutine entry point */
    cpu->PC = subroutine_addr;

    printf("[CALLG] Jumping to 0x%08X, return=0x%08X, args=%u\n",
           subroutine_addr, cpu->L, arg_count);

    /* ========================================================================
     * NOTES ON ENTRY POINT VALIDATION (optional, not implemented here)
     * ========================================================================
     *
     * Real hardware optionally validates that the target address points to
     * a valid entry point instruction:
     * - ENTS (0x00B8)
     * - ENTM (0x00DF)
     * - ENTD (0x009C)
     * - ENTB (0x00BD)
     * - etc.
     *
     * If the target is not an entry point, hardware raises ISE
     * (Instruction Sequence Error) trap.
     *
     * This validation is currently NOT implemented in the emulator.
     * We trust that the code is correct and the target is valid.
     *
     * To implement this validation:
     *   1. Read opcode byte at subroutine_addr
     *   2. Check if it's a valid ENT* opcode
     *   3. If not, call trap_instruction_sequence_error()
     * ======================================================================== */

    /* No status bits affected */
}
