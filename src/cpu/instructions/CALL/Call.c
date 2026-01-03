#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include "nd500_indirect.h"
#include "mon.h"
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

    /* Validate extra operand count matches arg_count */
    if (cpu->extra_operand_count != arg_count) {
        printf("[ERROR] CALL at PC=0x%08X: Expected %u extra operands, got %u\n",
               fi->address, arg_count, cpu->extra_operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* For MON calls, log argument details at DEBUG level */
    int is_mon_call = ((subroutine_addr >> 27) == 31);
    uint32_t mon_number = is_mon_call ? (subroutine_addr & 0x1FF) : 0;

    /* Process arguments from cpu->extra_operands (decoded by cpu_instr.c) */
    for (uint16_t i = 0; i < arg_count && i < ND500_MAX_OPERANDS; i++) {
        const Nd500OperandDecoded* arg_operand = &cpu->extra_operands[i];

        /* Debug: trace argument operands for MON calls */
        if (is_mon_call) {
            const char* mon_name = mon_get_name(mon_number);
            mon_log(MON_LOG_DEBUG, "MON %oB (%s) arg[%u]: mode=%d, ea=0x%08X",
                    mon_number, mon_name ? mon_name : "?",
                    i, arg_operand->mode, arg_operand->effective_address);
        }

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

    /* Calculate return address: address after this CALL instruction */
    uint32_t return_address = fi->address + fi->total_len;

    /* Store call information in CPU-internal state */
    cpu->pending_call_arg_count = arg_count;
    cpu->pending_call_return_address = return_address;

    /* Save return address in L register as well */
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

    if (indirect_result == INDIRECT_HANDLED) {
        /* SINTRAN MON call completed - return to caller, don't jump to entry */
        cpu->PC = resolved_addr;  /* = return_address */
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

    /* Check if a trap occurred during the fetch (e.g., page fault) */
    if (nd500_trap_occurred()) {
        printf("[CALL] Failed to fetch entry opcode from 0x%08X (program space)\n", resolved_addr);
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
        printf("[TRAP] CALL at PC=0x%08X: Target 0x%08X opcode=0x%02X is not an entry point (PROGRAM SPACE)\n",
               fi->address, resolved_addr, entry_opcode);
        trap_instruction_sequence_error(cpu, fi->address);
        return;
    }

    /* INDIRECT_DIRECT or INDIRECT_DOMAIN_SWITCH: Jump to resolved address */
    cpu->PC = resolved_addr;

    /* Call trap (CT) would be checked by trap system if enabled */
    /* Note: This is an ignorable trap that doesn't stop execution */
}
