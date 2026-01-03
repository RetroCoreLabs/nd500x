#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * ENTM instruction - CALL class
 *
 * Mnemonic: entm
 * Operands: 3
 * Opcode: 0x00DF (hex) / 0000337 (octal) / 223 (decimal)
 *
 * Operation: Enter Module (Initialize New Stack)
 *
 * Description:
 * ENTM is used for module initialization - starting a completely new independent
 * stack. It's the only entry point instruction that can be called cross-domain,
 * making it essential for OS kernel entry points and isolated program modules.
 *
 * A module is a major program component (like a segment, library, or subsystem)
 * that needs its own independent stack space separate from the calling code's stack.
 *
 * Key Difference from ENTS:
 * - ENTS: Extends current stack (new_b = old_b.SP, same TOS)
 * - ENTM: Starts NEW stack (new_b = <bottom of stack>, new TOS)
 *
 * Stack Independence:
 * ENTM creates a completely separate stack that can grow independently:
 * - Old stack: Continues at old TOS, preserves old TOS value
 * - New stack: Starts at <bottom of stack>, has its own TOS limit
 * - Old TOS saved on old stack (at old B.SP)
 * - New TOS set to <bottom of stack> + <total stack demand>
 *
 * Stack Layout Comparison:
 *   Before ENTM (old stack):
 *     [Frame 1] [Frame 2] [SP points here] ... [old TOS]
 *
 *   After ENTM (two stacks):
 *     Old stack: [Frame 1] [Frame 2] [saved old TOS] ... [old TOS]
 *     New stack: [new B] [SP points here] ... [new TOS = bottom + total]
 *
 * Same-Domain Operation:
 * When called within the same domain:
 * 1. Save old TOS value on the old stack (at old B.SP)
 * 2. Initialize new stack at <bottom of stack>
 * 3. Set new TOS = <bottom of stack> + <total stack demand>
 * 4. Create standard frame layout at new B
 * 5. Link back to old stack (B.PREVB = old B)
 *
 * Cross-Domain Operation (TODO):
 * When called across domains:
 * 1. Mark domain boundary (B.PREVB = 0, B.RETA = 0)
 * 2. Save old TOS, LL, HL, THA in domain information table
 * 3. Load new TOS, LL, HL, THA from new domain's DIT
 * 4. Initialize new stack in new domain
 *
 * Operand Structure:
 * - Operand[0]: Bottom of stack (read, word, DIRECT addressing)
 *   - Base address where new stack begins
 *   - Usually a static memory area allocated for the module
 *   - Addressing modes: DIRECT (not general operand)
 *   - Data type: Word (32-bit address)
 *
 * - Operand[1]: Stack demand main (read, word)
 *   - Initial stack pointer offset from bottom
 *   - How much stack space is immediately allocated
 *   - Addressing modes: CONSTANT, LOCAL, RECORD, etc.
 *   - Data type: Word (32-bit byte count)
 *
 * - Operand[2]: Total stack demand (read, word)
 *   - Maximum stack size (TOS limit)
 *   - Must be >= stack demand main
 *   - Addressing modes: CONSTANT, LOCAL, RECORD, etc.
 *   - Data type: Word (32-bit byte count)
 *
 * Stack Frame Layout (same as ENTS):
 *   Offset  Field    Description
 *   +0      PREVB    Previous B register (old B, or 0 for domain boundary)
 *   +4      RETA     Return address (or 0 for domain boundary)
 *   +8      SP       Stack pointer (bottom + stack_demand_main)
 *   +12     AUX      Auxiliary field (0 for ENTM)
 *   +16     N        Argument count
 *   +20     ARG1     First argument address
 *   +24     ARG2     Second argument address
 *   ...     ...      (additional arguments)
 *
 * Operation Steps:
 * 1. Validate operand count (must be 3)
 * 2. Validate pending call state (must be preceded by CALL/CALLG)
 * 3. Read operands: bottom of stack, stack demand main, total stack demand
 * 4. Validate stack demands (main < total, else STO trap)
 * 5. Save old TOS on old stack (at old B.SP)
 * 6. Initialize new stack frame at <bottom of stack>
 * 7. Set new B register to <bottom of stack>
 * 8. Set new TOS register to <bottom of stack> + <total stack demand>
 * 9. Copy argument addresses to new frame
 * 10. Clear pending call state
 *
 * Flag Behavior:
 * - All data status bits unaffected (S, Z, C, O, K)
 *
 * Trap Conditions:
 * - Addressing traps: Invalid address, page fault, protection violation
 * - Instruction Sequence Error (ISE): Not preceded by CALL/CALLG
 * - Stack Overflow (STO): stack_demand_main >= total_stack_demand
 *
 * Performance:
 * - Execution: ~25-30 cycles
 * - Variable depending on number of arguments
 * - Slower than ENTS due to TOS save/restore
 *
 * Key Characteristics:
 * - Starts new independent stack
 * - Can be called cross-domain
 * - Saves old TOS on old stack
 * - Updates TOS register to new stack limit
 * - 3 operands (bottom, demand main, demand total)
 * - Must be preceded by CALL/CALLG
 * - ISE trap if no pending call
 * - STO trap if stack demands invalid
 * - Frame layout identical to ENTS
 *
 * Common Use Cases:
 * - Operating system kernel entry from user mode
 * - Dynamic library/module initialization
 * - Subsystem isolation (separate stacks for safety)
 * - Cross-domain calls (OS services)
 * - Independent task stacks
 *
 * Typical Usage:
 *   Example 1: Module initialization
 *     CALL MODULE_INIT, 0
 *   MODULE_INIT:
 *     ENTM MODULE_STACK, 1000H, 8000H    ; New stack at MODULE_STACK
 *     ; Stack can grow up to 8000H bytes from MODULE_STACK
 *     ; Initial SP at MODULE_STACK + 1000H
 *
 *   Example 2: OS kernel entry
 *     CALL KERNEL_ENTRY, 2, SYSCALL_NUM, ARG1
 *   KERNEL_ENTRY:
 *     ENTM KERNEL_STACK, 2000H, 10000H   ; Independent kernel stack
 *     ; Kernel stack isolated from user stack
 *
 *   Example 3: Dynamic module loading
 *     ; Allocate stack space for module
 *     W1 GETMEM 10000H               ; Allocate 64KB
 *     CALL MODULE_START, 1, CONFIG
 *   MODULE_START:
 *     ENTM W1, 2000H, 10000H         ; New stack in allocated memory
 *
 * Warning:
 * Executing the same ENTM twice will overwrite the old stack at the same
 * location, potentially destroying return addresses and causing crashes!
 * Each module should have its own unique stack area.
 *
 * Notes:
 * - ENTM is the ONLY entry point that can cross domain boundaries
 * - Old TOS must be saved before switching stacks
 * - New TOS defines maximum stack growth
 * - Return via standard RET* instructions
 * - Cross-domain ENTM marks PREVB=0, RETA=0 (domain boundary marker)
 *
 * IMPLEMENTATION STATUS: FUNCTIONAL (Same-domain only)
 *
 * Current implementation:
 * - Same-domain ENTM fully implemented
 * - Saves old TOS on old stack
 * - Initializes new stack frame
 * - Updates TOS register to new stack limit
 * - Cross-domain ENTM marked TODO (requires DIT implementation)
 *
 * Related Instructions:
 * - CALL: Call subroutine with direct address
 * - CALLG: Call subroutine general (indirect)
 * - ENTS: Enter stack subroutine (extends current stack)
 * - ENTD: Enter domain subroutine (domain switch)
 * - RET: Return from subroutine
 *
 * Reference: ND-500 Reference Manual, Chapter 13.10, Chapter 3.2
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Entm.cs
 */
void nd500_instr_Entm(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Check if trace mode is enabled for debug output */
    int do_trace = nd500_dbg_get_trace_mode();

    /* ========================================================================
     * STACK FRAME FIELD OFFSETS (identical to ENTS)
     * ======================================================================== */
    const uint32_t OFFSET_PREVB = 0;   /* Previous B register */
    const uint32_t OFFSET_RETA  = 4;   /* Return address */
    const uint32_t OFFSET_SP    = 8;   /* Stack pointer */
    const uint32_t OFFSET_AUX   = 12;  /* Auxiliary field */
    const uint32_t OFFSET_N     = 16;  /* Argument count */
    const uint32_t OFFSET_ARG1  = 20;  /* First argument address */

    /* ========================================================================
     * STEP 1: VALIDATE OPERAND COUNT
     * ======================================================================== */
    if (fi->operand_count != 3) {
        printf("[ERROR] ENTM expects 3 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* ========================================================================
     * STEP 2: VALIDATE INSTRUCTION SEQUENCE (must follow CALL/CALLG)
     * ======================================================================== */
    if (cpu->pending_call_return_address == 0) {
        printf("[TRAP] ENTM at PC=0x%08X: Must be preceded by CALL/CALLG\n",
               fi->address);
        trap_instruction_sequence_error(cpu, fi->address);
        return;
    }

    /* ========================================================================
     * STEP 3: READ OPERANDS
     * ======================================================================== */
    /* Operand 0: Bottom of stack (DIRECT addressing, base of new stack) */
    uint32_t bottom_of_stack = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);

    /* Operand 1: Stack demand main (initial SP offset) */
    uint32_t stack_demand_main = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[1], ND500_DTYPE_WORD);

    /* Operand 2: Total stack demand (maximum stack size, TOS limit) */
    uint32_t total_stack_demand = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[2], ND500_DTYPE_WORD);

    if (do_trace) {
        printf("[ENTM] bottom=0x%08X, demand_main=0x%08X, total=0x%08X at PC=0x%08X\n",
               bottom_of_stack, stack_demand_main, total_stack_demand, fi->address);
    }

    /* ========================================================================
     * STEP 4: VALIDATE STACK DEMANDS
     * ======================================================================== */
    /* Stack overflow if main demand >= total demand */
    if (stack_demand_main >= total_stack_demand) {
        printf("[TRAP] ENTM at PC=0x%08X: Stack overflow - main demand 0x%08X >= total 0x%08X\n",
               fi->address, stack_demand_main, total_stack_demand);
        trap_stack_overflow(cpu, fi->address);
        return;
    }

    /* ========================================================================
     * STEP 5: SAVE OLD TOS ON OLD STACK
     * ========================================================================
     *
     * CRITICAL: Must save old TOS BEFORE switching stacks!
     *
     * Old stack continues to exist with its own TOS value. We save it at
     * the old B.SP location so it can be restored when returning to the
     * old stack.
     * ======================================================================== */
    uint32_t old_b = cpu->B;
    uint32_t old_tos = cpu->TOS;

    /* Read old B.SP to know where to save old TOS */
    uint32_t old_sp = nd500_read_memory_32(cpu, old_b + OFFSET_SP);

    /* Save old TOS at old SP location */
    nd500_write_memory_32(cpu, old_sp, old_tos);

    if (do_trace) {
        printf("  ENTM saved old TOS=0x%08X at old SP=0x%08X\n", old_tos, old_sp);
    }

    /* ========================================================================
     * STEP 6-8: INITIALIZE NEW STACK FRAME
     * ======================================================================== */
    uint32_t new_b = bottom_of_stack;

    /* B.PREVB = old B (link back to old stack) */
    nd500_write_memory_32(cpu, new_b + OFFSET_PREVB, old_b);

    /* B.RETA = return address */
    uint32_t return_addr = cpu->pending_call_return_address;
    nd500_write_memory_32(cpu, new_b + OFFSET_RETA, return_addr);

    /* Update L register with return address (standard calling convention) */
    cpu->L = return_addr;

    /* B.SP = bottom of stack + stack demand main */
    nd500_write_memory_32(cpu, new_b + OFFSET_SP, bottom_of_stack + stack_demand_main);

    /* B.AUX = 0 (not used for ENTM) */
    nd500_write_memory_32(cpu, new_b + OFFSET_AUX, 0);

    /* ========================================================================
     * STEP 9: SET NEW TOS REGISTER
     * ========================================================================
     *
     * This is KEY difference from ENTS!
     * New TOS defines maximum stack growth for the new independent stack.
     * ======================================================================== */
    cpu->TOS = bottom_of_stack + total_stack_demand;

    if (do_trace) {
        printf("  ENTM new TOS=0x%08X (bottom + total)\n", cpu->TOS);
    }

    /* ========================================================================
     * STEP 10: COPY ARGUMENTS TO NEW FRAME
     * ======================================================================== */
    /* B.N = argument count */
    uint32_t arg_count = cpu->pending_call_arg_count;
    nd500_write_memory_32(cpu, new_b + OFFSET_N, arg_count);

    /* Copy argument addresses */
    for (uint32_t i = 0; i < arg_count && i < ND500_MAX_OPERANDS; i++) {
        nd500_write_memory_32(cpu, new_b + OFFSET_ARG1 + (i * 4),
                             cpu->pending_call_arg_addresses[i]);
    }

    /* ========================================================================
     * STEP 11: UPDATE B REGISTER
     * ======================================================================== */
    cpu->B = new_b;

    /* ========================================================================
     * STEP 12: CLEAR PENDING CALL STATE
     * ======================================================================== */
    cpu->pending_call_return_address = 0;
    cpu->pending_call_arg_count = 0;

    if (do_trace) {
        printf("[ENTM] New stack initialized: B=0x%08X, TOS=0x%08X, args=%u\n",
               new_b, cpu->TOS, arg_count);
    }

    /* ========================================================================
     * NOTES ON CROSS-DOMAIN ENTM (not implemented)
     * ========================================================================
     *
     * For cross-domain calls, ENTM would:
     * 1. Detect domain change (compare calling domain with target domain)
     * 2. Mark domain boundary: B.PREVB = 0, B.RETA = 0
     * 3. Save old TOS, LL, HL, THA in domain information table (DIT)
     * 4. Load new TOS, LL, HL, THA from new domain's DIT
     * 5. Initialize new stack in new domain
     *
     * Domain boundary markers (PREVB=0, RETA=0) prevent stack unwinding
     * across domain boundaries during exceptions.
     *
     * To implement cross-domain ENTM:
     *   1. Add domain detection logic
     *   2. Implement DIT save/restore
     *   3. Mark boundary with zero PREVB/RETA
     * ======================================================================== */

    /* No status bits affected */
}
