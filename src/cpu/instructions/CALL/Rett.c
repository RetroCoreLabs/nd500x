#include "cpu_protos.h"
#include "machine_protos.h"
#include <stdio.h>

/**
 * RETT instruction - CALL class
 *
 * Mnemonic: rett
 * Operands: 0
 * Opcode: 0xFD3B (hex) / 0176473 (octal) / 64827 (decimal)
 *
 * Operation: Return from Trap Handler (Restore context and return)
 *
 * Description:
 * RETT is the LAST instruction executed in every trap handler routine. It completes
 * trap handling by restoring the full CPU context that was saved by ENTT and
 * returning control to the interrupted program.
 *
 * ENTT/RETT Pair:
 * ENTT and RETT form a matched pair for trap handler entry/exit:
 * - **ENTT** (Enter Trap): First instruction in trap handler
 *   * Saves full CPU context to PCB
 *   * Clears OTE to prevent recursive traps
 *   * Marks domain as "inside trap handler"
 * - **RETT** (Return from Trap): Last instruction in trap handler
 *   * Restores full CPU context from PCB
 *   * Restores OTE to re-enable traps
 *   * Returns to interrupted program
 *
 * Trap Handler Structure:
 *   TRAP_HANDLER:
 *     ENTT                     ; Save context, clear OTE
 *     ; ... handler body ...
 *     RETT                     ; Restore context, return
 *
 * Context Restoration:
 * RETT restores the processor state from the trap handler's local data field:
 * - Register block (39 words):
 *   * Main registers: PC, B, L
 *   * Integer registers: I1-I4
 *   * Floating-point registers: A1-A4
 *   * Extended registers: E1-E4
 *   * Status registers: ST1, ST2
 *   * Trap enable registers: OTE1, OTE2, MTE1, MTE2
 *   * Stack registers: TOS, LL, HL, THA
 * - Trapping P (program counter where trap occurred)
 * - Domain information (if cross-domain trap)
 *
 * Return Behavior:
 * - **Ignorable traps**: PC points to NEXT instruction (trap was handled, continue)
 * - **Non-ignorable/fatal traps**: PC points to FAULTING instruction (retry after fix)
 * - **Corrected traps**: PC points to faulting instruction (e.g., page fault fixed)
 *
 * Operand Structure:
 * - No operands (zero-operand instruction)
 *
 * Operation Steps:
 * 1. Validate operand count (must be 0)
 * 2. Get current domain's PCB (Process Control Block)
 * 3. Verify inside trap handler (ISE trap if not)
 * 4. Get saved context from PCB (ISE trap if missing)
 * 5. Log operation
 * 6. Clear trap bit in ST register
 * 7. Restore trap enable registers (OTE1, OTE2, MTE1, MTE2)
 * 8. Restore main registers (B, I1-I4, A1-A4)
 * 9. Switch back to original domain (if cross-domain trap)
 * 10. Mark as no longer in trap handler
 * 11. Return to saved PC
 *
 * Flag Behavior:
 * - All flags restored from saved context (via ST register)
 *
 * Trap Conditions:
 * - Addressing traps: Invalid address, page fault, protection violation
 * - Instruction Sequence Error (ISE): Not inside trap handler
 * - Instruction Sequence Error (ISE): No saved context available
 *
 * Performance:
 * - Execution: ~40-50 cycles
 * - Variable depending on context restore size
 * - One-time cost per trap handler exit
 *
 * Key Characteristics:
 * - MUST be last instruction in trap handler
 * - Zero operands
 * - Restores full CPU context from PCB
 * - Restores OTE to re-enable traps
 * - Pairs with ENTT for context save
 * - Part of ND-500 trap system architecture
 * - Requires PCB (Process Control Block) infrastructure
 * - Requires domain support
 * - ISE trap if not inside trap handler
 *
 * Common Use Cases:
 * - Trap handler exit point (ALWAYS last instruction)
 * - Exception handler cleanup and return
 * - Interrupt handler return
 * - System call handler return
 * - Page fault handler return (after fixing page)
 *
 * Typical Usage:
 *   Example 1: Page fault handler
 *     PAGE_FAULT_HANDLER:
 *       ENTT                     ; Save context, clear OTE
 *       ; Fix page fault...
 *       RETT                     ; Restore context, retry faulting instruction
 *
 *   Example 2: Division by zero handler
 *     DIV_ZERO_HANDLER:
 *       ENTT                     ; Save context, clear OTE
 *       ; Log error, set result to 0...
 *       RETT                     ; Restore context, continue to next instruction
 *
 *   Example 3: System call handler
 *     SYSCALL_HANDLER:
 *       ENTT                     ; Save context, clear OTE
 *       ; Handle syscall, set return value...
 *       RETT                     ; Restore context, return to caller
 *
 * Warning:
 * RETT MUST only be executed inside a trap handler! Executing it elsewhere will
 * cause ISE trap. RETT MUST be paired with ENTT - executing RETT without prior
 * ENTT will cause ISE trap due to missing saved context.
 *
 * Notes:
 * - ENTT and RETT form a matched pair
 * - Always paired with ENTT at trap handler entry
 * - OTE restoration re-enables trap processing
 * - Saved context includes all processor state
 * - Context restore is atomic (cannot be interrupted)
 * - PCB structure varies by domain
 * - Cross-domain traps switch back to original domain
 * - Trap bit is cleared in ST register
 *
 * IMPLEMENTATION STATUS: STUB (Requires full trap system infrastructure)
 *
 * Current implementation:
 * - Validates operand count
 * - Logs trap handler return
 * - Full context restore REQUIRES:
 *   * PCB (Process Control Block) per domain
 *   * DomainContext structure
 *   * Trap enable registers (OTE1, OTE2, MTE1, MTE2)
 *   * Domain management infrastructure
 *   * Saved context tracking
 *   * InsideTrapHandler flag
 *
 * To implement RETT fully:
 * 1. Implement PCB structure and management
 * 2. Implement domain system (CAD, mother domain, etc.)
 * 3. Implement trap enable registers (OTE1, OTE2, MTE1, MTE2)
 * 4. Implement DomainContext save/restore
 * 5. Implement InsideTrapHandler flag tracking
 * 6. Integrate with trap dispatch mechanism
 * 7. Implement trap bit clearing in ST register
 *
 * Related Instructions:
 * - ENTT: Enter trap handler (saves context)
 * - TRAP: Explicit software trap
 * - WAIT: Wait for interrupt
 *
 * Reference: ND-500 Reference Manual, Chapter 6.4
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Rett.cs
 */
void nd500_instr_Rett(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* ========================================================================
     * STEP 1: VALIDATE OPERAND COUNT
     * ======================================================================== */
    if (fi->operand_count != 0) {
        printf("[ERROR] RETT expects 0 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* ========================================================================
     * RETT - Return from Trap Handler
     * ========================================================================
     *
     * RETT is the last instruction in every trap handler routine.
     * It restores the complete register block from the trap frame that
     * ENTT saved at THA + 256, then returns to the trapping instruction.
     *
     * From ND-500 Reference Manual Section 13.11:
     *   "The register block is loaded from B.arg2..B.arg40."
     *
     * Current B register points to trap handler local data field (THA + 256).
     * Register block layout in trap frame:
     *   B+20   arg1     Trapping P (PC of trapped instruction)
     *   B+24   arg2     P register (return PC)
     *   B+28   arg3     L register
     *   B+32   arg4     B register (pre-trap value)
     *   B+36   arg5     R register
     *   B+40   arg6     I1
     *   ... etc (see ENTT for full layout)
     *   B+140  arg31    OTE1
     *   B+144  arg32    OTE2
     * ======================================================================== */

    /* Verify we're in trap handler context */
    if (!cpu->in_trap_handler) {
        printf("[ERROR] RETT at PC=0x%08X: Not in trap handler\n", fi->address);
        trap_instruction_sequence_error(cpu, fi->address);
        return;
    }

    /* B currently points to trap frame (THA + 256) */
    uint32_t trap_frame_base = cpu->B;

    printf("[RETT] Trap %d: Restoring from trap frame at 0x%08X\n",
           cpu->trap_number, trap_frame_base);

    /* ========================================================================
     * Read saved register values from trap frame
     * ======================================================================== */

    /* Read key registers from trap frame */
    uint32_t saved_PC = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 24, 0, 0));  /* arg2: return PC */
    uint32_t saved_L = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 28, 0, 0));   /* arg3: L */
    uint32_t saved_B = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 32, 0, 0));   /* arg4: B */
    uint32_t saved_R = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 36, 0, 0));   /* arg5: R */

    /* I1-I4 */
    uint32_t saved_I1 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 40, 0, 0));
    uint32_t saved_I2 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 44, 0, 0));
    uint32_t saved_I3 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 48, 0, 0));
    uint32_t saved_I4 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 52, 0, 0));

    /* A1-A4 */
    uint32_t saved_A1 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 56, 0, 0));
    uint32_t saved_A2 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 60, 0, 0));
    uint32_t saved_A3 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 64, 0, 0));
    uint32_t saved_A4 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 68, 0, 0));

    /* E1-E4 */
    uint32_t saved_E1 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 72, 0, 0));
    uint32_t saved_E2 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 76, 0, 0));
    uint32_t saved_E3 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 80, 0, 0));
    uint32_t saved_E4 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 84, 0, 0));

    /* ST1, ST2 */
    uint32_t saved_ST1 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 88, 0, 0));
    uint32_t saved_ST2 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 92, 0, 0));

    /* PS */
    uint32_t saved_PS = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 96, 0, 0));

    /* TOS, LL, HL */
    uint32_t saved_TOS = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 100, 0, 0));
    uint32_t saved_LL = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 104, 0, 0));
    uint32_t saved_HL = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 108, 0, 0));

    /* THA - don't restore, keep current */
    /* CED, CAD - restore for domain context */
    uint32_t saved_CED = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 116, 0, 0));
    uint32_t saved_CAD = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 120, 0, 0));

    /* OTE1, OTE2 - Per ND-500 manual, OTE should be loaded from DIT, not register block.
     * For now we use the OTE saved by invoke_trap_handler() before ENTT cleared them.
     * This preserves the pre-trap OTE which is the correct behavior. */
    uint32_t saved_OTE1 = cpu->trap_saved_OTE1;
    uint32_t saved_OTE2 = cpu->trap_saved_OTE2;

    /* CTE1, CTE2 */
    uint32_t saved_CTE1 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 148, 0, 0));
    uint32_t saved_CTE2 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 152, 0, 0));

    /* MTE1, MTE2 */
    uint32_t saved_MTE1 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 156, 0, 0));
    uint32_t saved_MTE2 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 160, 0, 0));

    /* TEMM1, TEMM2 */
    uint32_t saved_TEMM1 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 164, 0, 0));
    uint32_t saved_TEMM2 = nd500_bus_read32(cpu->machine,
        nd500_mmu_translate(cpu, trap_frame_base + 168, 0, 0));

    printf("[RETT]   Restoring: B=0x%08X L=0x%08X TOS=0x%08X PC=0x%08X\n",
           saved_B, saved_L, saved_TOS, saved_PC);

    /* ========================================================================
     * Step 4: Clear the specific trap status bit before restoring
     * (ENTT already cleared it but we ensure it stays cleared)
     * ======================================================================== */
    uint64_t trapBit = 1ULL << cpu->trap_number;
    if (cpu->trap_number < 32) {
        saved_ST1 &= ~(uint32_t)(trapBit & 0xFFFFFFFF);
    } else {
        saved_ST2 &= ~(uint32_t)(trapBit >> 32);
    }

    /* ========================================================================
     * Restore all registers
     * ======================================================================== */

    /* Core registers */
    cpu->L = saved_L;
    cpu->B = saved_B;
    cpu->R = saved_R;

    /* Integer registers */
    cpu->I[0] = saved_I1;
    cpu->I[1] = saved_I2;
    cpu->I[2] = saved_I3;
    cpu->I[3] = saved_I4;

    /* Float registers */
    cpu->A[0] = saved_A1;
    cpu->A[1] = saved_A2;
    cpu->A[2] = saved_A3;
    cpu->A[3] = saved_A4;

    /* Extension registers */
    cpu->E[0] = saved_E1;
    cpu->E[1] = saved_E2;
    cpu->E[2] = saved_E3;
    cpu->E[3] = saved_E4;

    /* Status registers (with trap bit cleared) */
    cpu->ST1 = saved_ST1;
    cpu->ST2 = saved_ST2;

    /* Process segment */
    cpu->PS = saved_PS;

    /* Stack registers */
    cpu->TOS = saved_TOS;
    cpu->LL = saved_LL;
    cpu->HL = saved_HL;

    /* Domain registers */
    cpu->CED = saved_CED;
    cpu->CAD = saved_CAD;

    /* Trap enable registers */
    cpu->OTE1 = saved_OTE1;
    cpu->OTE2 = saved_OTE2;
    cpu->CTE1 = saved_CTE1;
    cpu->CTE2 = saved_CTE2;
    cpu->MTE1 = saved_MTE1;
    cpu->MTE2 = saved_MTE2;
    cpu->TEMM1 = saved_TEMM1;
    cpu->TEMM2 = saved_TEMM2;

    /* ========================================================================
     * Return to saved PC (retry the trapping instruction)
     * ======================================================================== */
    cpu->PC = saved_PC;

    /* Clear trap handler flag */
    cpu->in_trap_handler = false;

    printf("[RETT] Trap %d handler complete, returning to PC=0x%08X\n",
           cpu->trap_number, cpu->PC);
}
