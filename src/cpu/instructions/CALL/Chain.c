#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include "nd500_settings.h"   /* emulator knobs, as plain fields */

/**
 * CHAIN instruction - CALL class
 *
 * Mnemonic: chain
 * Operands: 3
 * Opcode: 0xFD6C (hex) / 0175554 (octal) / 64876 (decimal)
 *
 * Operation: Load Address of Multilevel Chain
 *
 * Description:
 * Traverses a static link chain through nested procedure scopes and loads the
 * base address of the target scope into a register. This instruction is essential
 * for block-structured languages (Pascal, Algol, etc.) that allow nested procedures
 * to access variables declared in outer scopes.
 *
 * Static Link Chain Architecture:
 * In block-structured languages, each procedure activation record (stack frame)
 * contains a "static link" pointer to the enclosing scope's frame. This creates
 * a chain of frames following lexical scope nesting, separate from the dynamic
 * call chain (return addresses).
 *
 *   Example nested procedures:
 *     procedure A;
 *       var x: integer;
 *       procedure B;
 *         var y: integer;
 *         procedure C;
 *           begin
 *             x := 42;  // Access A's variable from C - traverse 2 levels!
 *           end;
 *
 *   Stack layout when C executes:
 *     Frame C: [static link -> B] [local vars] [return addr]
 *     Frame B: [static link -> A] [local vars] [return addr]
 *     Frame A: [static link -> ?] [x: integer] [return addr]
 *
 *   To access 'x' from C:
 *     CHAIN B, STATIC_LINK_OFFSET, 2  -> Loads address of A's frame
 *     Then use A-relative addressing to access 'x'
 *
 * Algorithm:
 *   1. Load starting address (usually current B register)
 *   2. For each level (1..levels):
 *      a. Read static link at (currentAddr + offset)
 *      b. If link is zero: trap IOV, set K flag
 *      c. Follow link: currentAddr = staticLink
 *   3. Store final address in target register
 *
 * Operand Structure:
 * - Operand[0]: Starting address (read, word)
 *   - Usually the current B register value
 *   - Can be any address to start chain traversal
 *   - Addressing modes: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
 *   - Data type: Word (32-bit address)
 *
 * - Operand[1]: Offset to static link field (read, word)
 *   - Byte offset within frame to static link pointer
 *   - Usually B-relative (e.g., B.STATIC_LINK = B + 16)
 *   - Addressing modes: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
 *   - Data type: Word (32-bit offset)
 *
 * - Operand[2]: Number of levels to traverse (read, word)
 *   - How many static links to follow (0..n)
 *   - Computed by compiler from lexical scope difference
 *   - Addressing modes: LOCAL, RECORD, CONSTANT, REGISTER, PRE_INDEXED, ABSOLUTE
 *   - Data type: Word (32-bit count)
 *
 * Operation Steps:
 * 1. Validate operand count (must be 3)
 * 2. Read starting address from operand[0]
 * 3. Read offset to static link from operand[1]
 * 4. Read number of levels from operand[2]
 * 5. Validate levels >= 0 (negative is IOV trap)
 * 6. Special case: levels == 0 -> load address (LADDR equivalent)
 * 7. For i = 0 to levels-1:
 *    a. Read next link: memory[currentAddr + offset]
 *    b. If link == 0: set K flag, store current addr, trap IOV
 *    c. currentAddr = nextLink
 * 8. Store final address in target register
 * 9. Update S flag based on address sign bit
 * 10. Clear K flag (successful traversal)
 *
 * Flag Behavior:
 * - S (Sign): Set if final address has sign bit set (bit 31), cleared otherwise
 * - K (Error): Set if zero link encountered during traversal
 * - Z, C, O: Unaffected
 *
 * Trap Conditions:
 * - Addressing traps: Invalid address, page fault, protection violation
 * - Illegal Operand Value (IOV): Negative level count
 * - Illegal Operand Value (IOV): Zero link encountered before reaching target level
 *
 * Performance:
 * - Execution: ~10 + (5 x levels) cycles
 * - Variable depending on chain depth
 * - Typically 1-3 levels in practice
 *
 * Key Characteristics:
 * - Essential for block-structured language implementation
 * - Traverses static link chain for lexical scope access
 * - 3 operands: start address, link offset, level count
 * - IOV trap on negative levels or zero links
 * - K flag indicates premature chain termination
 * - S flag reflects final address sign
 * - Levels == 0 is equivalent to LADDR
 *
 * Common Use Cases:
 * - Pascal/Algol nested procedure variable access
 * - Upward funarg problem (accessing outer scope)
 * - Display register simulation
 * - Closure implementation in functional languages
 *
 * Typical Usage:
 *   Example 1: Access variable 2 scopes up
 *     W1 CHAIN B, 16, 2      ; Follow 2 static links from B+16
 *     LOAD X, W1.OUTER_VAR   ; Access variable in outer scope
 *
 *   Example 2: Load address without traversal
 *     W2 CHAIN B, 0, 0       ; Equivalent to LADDR B -> W2
 *
 *   Example 3: Dynamic level computation
 *     W3 CHAIN FRAME_PTR, STATIC_OFFSET, B.LEVEL_DIFF
 *
 * Notes:
 * - Static links are separate from dynamic return addresses
 * - Chain follows lexical scope, not call stack
 * - Zero link indicates stack corruption or compiler error
 * - Modern architectures use display registers instead
 * - Critical for Pascal, Algol-60, Algol-68 implementations
 *
 * IMPLEMENTATION STATUS: FUNCTIONAL (Re-implemented from fixed C#)
 *
 * Previous implementation was based on broken C# code. This version is
 * re-implemented from the corrected C# reference with proper:
 * - Zero link detection and K flag setting
 * - Levels == 0 special case handling
 * - S flag update based on final address sign bit
 * - Comprehensive static link chain documentation
 *
 * Related Instructions:
 * - LADDR: Load address (equivalent to CHAIN with levels=0)
 * - ENTS: Enter stack subroutine (creates stack frame with static link)
 * - CALL: Call subroutine (initiates subroutine call)
 *
 * Reference: ND-500 Reference Manual, Section 15.7
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Chain.cs (fixed)
 */
/* Once-latched env flag for CHAIN tracing (ND500X_CHAINDBG). The traversal
 * prints used to be unconditional, but a zero link is a NORMAL runtime event
 * (end-of-list walk with a large level count, e.g. LED) - unconditional
 * prints flooded program output. */
static int chain_dbg(void) {
    static int latch = -1;
    if (latch < 0) latch = nd500_settings()->chaindbg;
    return latch;
}

void nd500_instr_Chain(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* ========================================================================
     * STEP 1: VALIDATE OPERAND COUNT
     * ======================================================================== */
    if (fi->operand_count != 3) {
        printf("[ERROR] CHAIN expects 3 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* ========================================================================
     * STEP 2-4: READ OPERANDS
     * ======================================================================== */
    /* Operand 0: Starting address. Per the manual (section 15.7) this is typed <address/aa/W> -
     * an ADDRESS operand, exactly like LADDR's operand. CHAIN's operation is "<address> -> Wn"
     * then follow the chain, so operand 0 contributes its EFFECTIVE ADDRESS, not the memory value
     * stored at it. This used to call nd500_read_operand_value (read the value AT the address = 0),
     * so I1 came out 0 while the microword (correct) loaded the EA, e.g. B+100 = 0x2064. Resolve
     * the address operand exactly as Laddr.c does (constant modes carry the value, memory modes use
     * effective_address). */
    uint32_t start_address;
    {
        const Nd500OperandDecoded* op0 = &fi->operands[0];
        if (op0->mode == ND500_ADDR_CONSTANT_SHORT) {
            start_address = op0->address_code & 0x3F;
        } else if (op0->mode == ND500_ADDR_CONSTANT) {
            switch (op0->data_len) {
                case 1: start_address = op0->data[0]; break;
                case 2: start_address = (uint32_t)((op0->data[0] << 8) | op0->data[1]); break;
                case 4: start_address = (uint32_t)((op0->data[0] << 24) | (op0->data[1] << 16) |
                                                    (op0->data[2] << 8) | op0->data[3]); break;
                default: start_address = 0; break;
            }
        } else {
            start_address = op0->effective_address;
        }
    }

    /* Operand 1: Offset to static link field within frame */
    uint32_t static_link_offset = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);

    /* Operand 2: Number of levels to traverse */
    int32_t levels = (int32_t)nd500_read_operand_value(cpu, &fi->operands[2], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    if (chain_dbg()) printf("[CHAIN] Start: addr=0x%08X, offset=%u, levels=%d, target=W%u at PC=0x%08X\n",
           start_address, static_link_offset, levels, fi->target_register, fi->address);

    /* ========================================================================
     * STEP 5: VALIDATE LEVELS (must be >= 0)
     * ======================================================================== */
    if (levels < 0) {
        if (chain_dbg()) printf("[CHAIN] illegal operand value - negative levels %d at PC=0x%08X\n",
               levels, fi->address);
        /* Illegal operand VALUE (IOV, bit 16) - an ignorable After-class trap,
         * not IOS: the operand specifier is fine, its VALUE is out of range
         * (manual ND-05.009.4 section 15.7 + Table 10). */
        raise_trap(cpu, TRAP_IOV, fi->address, 0);
        return;
    }

    /* ========================================================================
     * STEP 6: SPECIAL CASE - LEVELS == 0 (equivalent to LADDR)
     * ======================================================================== */
    if (levels == 0) {
        /* Just load the starting address into target register */
        nd500_write_integer_register(cpu, fi->target_register, start_address);

        /* Set S flag based on sign bit of address */
        if (start_address & 0x80000000) {
            nd500_set_flag(cpu, ND500_FLAG_S);
        } else {
            nd500_clear_flag(cpu, ND500_FLAG_S);
        }

        if (chain_dbg()) printf("[CHAIN] levels=0 (LADDR equivalent): loaded 0x%08X to W%u at PC=0x%08X\n",
               start_address, fi->target_register, fi->address);
        return;
    }

    /* ========================================================================
     * STEP 7: TRAVERSE STATIC LINK CHAIN
     * ======================================================================== */
    uint32_t current_addr = start_address;

    for (int32_t i = 0; i < levels; i++) {
        /* Read the static link: memory[currentAddr + offset] */
        uint32_t next_link = nd500_read_memory_32(cpu, current_addr + static_link_offset);

        if (chain_dbg()) printf("  CHAIN level %d/%d: addr=0x%08X, link@0x%08X = 0x%08X\n",
               i + 1, levels, current_addr, current_addr + static_link_offset, next_link);

        /* Check for zero link (premature chain termination). Manual 15.7:
         * "If the next link in the chain is zero, the operation is terminated,
         * Wn will contain the last element in the link (pointing to a zero
         * location) and the K flag is set. This will also cause an illegal
         * operand value trap condition." IOV is an ignorable After-class trap:
         * the instruction COMPLETES (register written, K set) and execution
         * resumes at the next instruction - LED walks a possibly-empty list
         * with levels=0x7FFFFFFF and relies on exactly this to detect the end. */
        if (next_link == 0) {
            if (chain_dbg()) printf("[CHAIN] zero link at level %d/%d at PC=0x%08X\n",
                   i + 1, levels, fi->address);

            /* Set K flag to indicate zero link encountered */
            nd500_set_flag(cpu, ND500_FLAG_K);

            /* Store current address (pointing to zero link location) in target register */
            nd500_write_integer_register(cpu, fi->target_register, current_addr);

            raise_trap(cpu, TRAP_IOV, fi->address, 0);
            return;
        }

        /* Follow the static link to next scope */
        current_addr = next_link;
    }

    /* ========================================================================
     * STEP 8-10: STORE RESULT AND UPDATE FLAGS
     * ======================================================================== */
    /* Store final address in target register */
    nd500_write_integer_register(cpu, fi->target_register, current_addr);

    /* Set S flag based on sign bit of final address */
    if (current_addr & 0x80000000) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }

    /* Clear K flag (successful traversal, no zero links) */
    nd500_clear_flag(cpu, ND500_FLAG_K);

    if (chain_dbg()) printf("[CHAIN] Completed: traversed %d levels, final addr=0x%08X in W%u, S=%d at PC=0x%08X\n",
           levels, current_addr, fi->target_register,
           (current_addr & 0x80000000) ? 1 : 0, fi->address);
}
