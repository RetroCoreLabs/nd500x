/*
 * Callg.c - ND-500 CALLG instruction (CALL class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "instructions_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include "nd500_indirect.h"
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
 * - Execution: ~15 + (3 x arg_count) cycles
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
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Callg.cs
 */
void nd500_instr_Callg(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Check if trace mode is enabled for debug output */
    int do_trace = nd500_dbg_get_trace_mode();

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

    /* Abort if either operand read page-faulted - see the identical guard and
     * the full explanation in Call.c. Continuing would clobber the freshly
     * installed trap-handler PC with a garbage target. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    if (do_trace) {
        printf("[CALLG] Address=0x%08X (via general operand), args=%u at PC=0x%08X\n",
               subroutine_addr, arg_count, fi->address);
    }

    /* ========================================================================
     * STEP 4: VALIDATE EXTRA OPERAND COUNT
     * ======================================================================== */
    /* Extra operands (arguments) are stored in cpu->extra_operands by decoder */
    if (cpu->extra_operand_count != arg_count) {
        printf("[ERROR] CALLG operand count mismatch: expected %u extra operands, got %u at PC=0x%08X\n",
               arg_count, cpu->extra_operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* ========================================================================
     * STEP 5: PROCESS ARGUMENTS FROM CPU EXTRA OPERANDS BUFFER
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

    for (uint16_t i = 0; i < arg_count && i < TRAP_SEQ_MAXARG; i++) {
        const Nd500OperandDecoded* arg_operand = &cpu->extra_operands[i];

        /* Validate: Arguments MUST be memory operands, not constants.
         *
         * The manual is explicit for both CALL and CALLG: "<argn> operands of
         * type register or constant will cause an illegal operand specifier
         * trap condition, as neither registers nor constants have an address in
         * data memory." Trap here exactly as Call.c does - substituting address
         * 0 and continuing hands the callee a null pointer for that argument
         * and corrupts the frame silently. */
        if (arg_operand->mode == ND500_ADDR_CONSTANT ||
            arg_operand->mode == ND500_ADDR_CONSTANT_SHORT) {
            ND500X_TRAPLOG("[TRAP] CALLG at PC=0x%08X: Argument %u is constant (mode=%u), must be memory operand\n",
                   fi->address, i + 1, arg_operand->mode);
            trap_illegal_operand(cpu, fi->address);
            return;
        }

        /* Store effective address of this argument */
        /* The subroutine will use this address to access the argument */
        cpu->pending_call_arg_addresses[i] = arg_operand->effective_address;

        /* Check if this is a MON call (segment 31) */
        if (do_trace) {
            if ((subroutine_addr >> 27) == 31) {
                printf("[CALLG MON] arg[%u]: mode=%d, addr_code=0x%02X, ea=0x%08X, B=0x%08X, R=0x%08X\n",
                       i, arg_operand->mode, arg_operand->address_code,
                       arg_operand->effective_address, cpu->B, cpu->R);
            } else {
                printf("  CALLG arg[%u]: addr=0x%08X (mode=%u)\n",
                       i, arg_operand->effective_address, arg_operand->mode);
            }
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

    /* Calculate return address: address after this CALLG instruction */
    uint32_t return_address = fi->address + fi->total_len;

    /* Store call information in CPU-internal state */
    cpu->pending_call_arg_count = arg_count;
    cpu->pending_call_return_address = return_address;

    /* ========================================================================
     * STEP 7-8: SAVE RETURN ADDRESS AND JUMP TO SUBROUTINE
     * ======================================================================== */

    /* Save return address in L register (standard ND-500 calling convention) */
    cpu->L = return_address;

    /* Check for indirect segment call (including SINTRAN MON calls) */
    uint32_t resolved_addr;
    int indirect_result = nd500_check_indirect_call(
        cpu, subroutine_addr, arg_count,
        cpu->pending_call_arg_addresses, fi->address, &resolved_addr);

    if (indirect_result == INDIRECT_ERROR || indirect_result == INDIRECT_BREAK) {
        /* Error, halt, or break requested - PC set to return address */
        cpu->PC = resolved_addr;
        return;
    }

    if (indirect_result == INDIRECT_WAIT) {
        /* Blocking read had no input - PC rewound to THIS CALLG so the MON call
         * retries on resume. Run loop already stopped with STOP_WAIT_INPUT. */
        cpu->PC = resolved_addr;  /* = fi->address (this instruction) */
        return;
    }

    if (indirect_result == INDIRECT_HANDLED) {
        /* SINTRAN MON call completed - return to caller, don't jump to entry */
        cpu->PC = resolved_addr;  /* = return_address */
        /* The MON call was serviced by the emulator, so NO entry-point ENTS ran to
         * consume the pending-call state. A normal CALL/ENTS pair clears it in
         * ENTS step 11; here we must clear it too. Otherwise the NEXT real ENTS
         * reads this stale return address + arg count and writes them into its
         * frame's RETA/N (and copies stale arg EAs), corrupting the frame chain -
         * observed as the stack leaking (frames never fully deallocate) since the
         * MON-heavy console-output path runs one MON per character. [leak fix] */
        cpu->pending_call_return_address = 0;
        cpu->pending_call_arg_count = 0;
        if (do_trace) {
            printf("[CALLG] MON call completed, returning to 0x%08X\n", resolved_addr);
        }
        return;
    }

    /* ========================================================================
     * ENTRY POINT VALIDATION
     * ========================================================================
     *
     * CRITICAL: Must read opcode from PROGRAM space, not DATA space!
     * On ND-500, program and data have separate capability tables (PMON vs DMON).
     * Using nd500_fetch_memory_8() ensures we read from program space.
     *
     * Valid entry point opcodes:
     * - ENTS   (0xB8) - Enter stack subroutine
     * - ENTSN  (0xBA) - Enter stack subroutine, no display
     * - ENTT   (0xBC) - Enter stack subroutine, timer
     * - ENTB   (0xBD) - Enter stack subroutine, block
     * - ENTD   (0x9C) - Enter domain subroutine
     * - ENTF   (0xDD) - Enter function (no arguments)
     * - ENTFN  (0xDE) - Enter function, no display
     * - ENTM   (0xDF) - Enter masked subroutine
     * ======================================================================== */

    /* Read opcode from PROGRAM space (not data space!) */
    uint8_t entry_opcode = nd500_fetch_memory_8(cpu, resolved_addr);

    /* Check if a trap occurred during the fetch (e.g., page fault). The trap
     * handler is invoked synchronously and clears the trap state, so also
     * check the per-instruction abort flag - otherwise a demand-paging fault
     * on the entry fetch falls through with the UNTRANSLATED address's bytes
     * (kernel physical memory) and raises a bogus ISE on a valid target. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Validate entry point opcode */
    int is_valid_entry = 0;
    switch (entry_opcode) {
        case 0xB8:  /* ENTS - Enter stack subroutine */
        case 0xBA:  /* ENTSN - Enter stack subroutine, no display */
        case 0xBC:  /* ENTT - Enter stack subroutine, timer */
        case 0xBD:  /* ENTB - Enter stack subroutine, block */
        case 0x9C:  /* ENTD - Enter domain subroutine */
        case 0xDD:  /* ENTF - Enter function (no arguments) */
        case 0xDE:  /* ENTFN - Enter function, no display */
        case 0xDF:  /* ENTM - Enter masked subroutine */
            is_valid_entry = 1;
            break;
        default:
            is_valid_entry = 0;
            break;
    }

    if (!is_valid_entry) {
        ND500X_TRAPLOG("[TRAP] CALLG at PC=0x%08X: Target 0x%08X opcode=0x%02X is not an entry point (PROGRAM SPACE)\n",
               fi->address, resolved_addr, entry_opcode);
        trap_instruction_sequence_error(cpu, fi->address);
        return;
    }

    /* INDIRECT_DIRECT or INDIRECT_DOMAIN_SWITCH: Jump to resolved address */
    cpu->PC = resolved_addr;

    if (do_trace) {
        printf("[CALLG] Jumping to 0x%08X, return=0x%08X, args=%u (entry opcode=0x%02X)\n",
               resolved_addr, cpu->L, arg_count, entry_opcode);
    }

    /* No status bits affected */
}
