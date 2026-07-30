#include "cpu_protos.h"
#include "machine_protos.h"
#include "nd500_mmu.h"
#include <stdio.h>
#include <stdlib.h>   /* getenv() - without this the implicit int prototype
                       * truncates the returned char* to 32 bits and the env
                       * check derefs a wild pointer (SIGSEGV in ENTT). */

/**
 * ENTT instruction - CALL class
 *
 * Mnemonic: entt
 * Operands: 0
 * Opcode: 0xFD3A (hex) / 0176472 (octal) / 64826 (decimal)
 *
 * Operation: Enter Trap Handler (Set up trap handler context)
 *
 * Description:
 * ENTT is the FIRST instruction executed in every trap handler routine. It completes
 * the trap handling setup that was initiated by the trap mechanism, performing full
 * CPU context save and clearing trap enables to prevent recursive traps.
 *
 * Trap Handling Sequence:
 * The ND-500 trap system uses a two-phase approach to minimize overhead and enable
 * flexible trap handling:
 *
 * 1. **Trap Occurs**: Hardware/software detects trap condition
 * 2. **InvokeTrapHandler()**: System finds handler address and jumps to it
 *    - Minimal state save (just enough to identify trap)
 *    - Sets pending trap information in PCB
 *    - Jumps to trap handler entry point
 * 3. **ENTT** (THIS INSTRUCTION): Saves full CPU context and clears OTE
 *    - Saves all registers to PCB (PC, B, I1-I4, A1-A4, ST, etc.)
 *    - Saves trap enable registers (OTE1, OTE2, MTE1, MTE2)
 *    - Clears OTE to prevent recursive traps during handler
 *    - Marks domain as "inside trap handler"
 * 4. **Trap Handler Body**: Executes user trap handling code
 * 5. **RETT**: Restores context and returns from trap
 *
 * Why This Design?
 * - **Performance**: InvokeTrapHandler() is fast (minimal save)
 * - **Flexibility**: Handler can examine trap info before full save
 * - **Correctness**: ENTT ensures atomic context save
 * - **Reentrancy**: Clearing OTE prevents nested traps
 *
 * Local Data Field Structure (created by ENTT):
 * The trap handler's local data field contains:
 *   - Local data field heading (5 words)
 *   - Trapping P (program counter where trap occurred) (1 word)
 *   - Copy of register block (39 words):
 *     * Main registers: PC, B, L
 *     * Integer registers: I1-I4
 *     * Floating-point registers: A1-A4
 *     * Extended registers: E1-E4
 *     * Status registers: ST1, ST2
 *     * Trap enable registers: OTE1, OTE2, MTE1, MTE2
 *     * Stack registers: TOS, LL, HL, THA
 *   - Local data area (variable size)
 *
 * Saved Context (all in PCB):
 * - Program counter (PC/P register)
 * - Base register (B)
 * - Integer registers (I1-I4)
 * - Floating-point registers (A1-A4)
 * - Status register (ST)
 * - Trap enable registers (OTE1, OTE2, MTE1, MTE2)
 * - Domain information (CAD, mother domain)
 * - Trapping PC (where trap occurred)
 * - Trap condition and trap bit
 *
 * Operand Structure:
 * - No operands (zero-operand instruction)
 *
 * Operation Steps:
 * 1. Validate operand count (must be 0)
 * 2. Get current domain's PCB (Process Control Block)
 * 3. Retrieve pending trap information from PCB:
 *    - Trap number
 *    - Trap bit (identifies trap type)
 *    - Trapping PC (where trap occurred)
 * 4. Save full CPU context to PCB:
 *    - All general-purpose registers (PC, B, I1-I4, A1-A4)
 *    - Status register (ST)
 *    - Trap enable registers (OTE1, OTE2, MTE1, MTE2)
 * 5. Save OTE before clearing (for RETT to restore)
 * 6. Clear OTE (prevent recursive traps during handler execution)
 * 7. Mark domain as "inside trap handler"
 * 8. Continue to trap handler body
 *
 * Flag Behavior:
 * - All data status bits unaffected (S, Z, C, O, K)
 *
 * Trap Conditions:
 * - Addressing traps: Invalid address, page fault, protection violation
 *
 * Performance:
 * - Execution: ~40-50 cycles
 * - Variable depending on context save size
 * - One-time cost per trap handler entry
 *
 * Key Characteristics:
 * - MUST be first instruction in trap handler
 * - Zero operands
 * - Saves full CPU context to PCB
 * - Clears OTE to prevent recursive traps
 * - Pairs with RETT for context restore
 * - Part of ND-500 trap system architecture
 * - Requires PCB (Process Control Block) infrastructure
 * - Requires domain support
 *
 * Common Use Cases:
 * - Trap handler entry point (ALWAYS first instruction)
 * - Exception handler setup
 * - Interrupt handler setup
 * - System call handler entry
 * - Page fault handler entry
 *
 * Typical Usage:
 *   Example 1: Page fault handler
 *     PAGE_FAULT_HANDLER:
 *       ENTT                     ; Save context, clear OTE
 *       ; Handle page fault...
 *       RETT                     ; Restore context, return
 *
 *   Example 2: Division by zero handler
 *     DIV_ZERO_HANDLER:
 *       ENTT                     ; Save context, clear OTE
 *       ; Log error, fix divisor, etc.
 *       RETT                     ; Restore context, return
 *
 *   Example 3: System call handler
 *     SYSCALL_HANDLER:
 *       ENTT                     ; Save context, clear OTE
 *       ; Decode syscall number, dispatch...
 *       RETT                     ; Restore context, return
 *
 * Warning:
 * ENTT MUST be the first instruction in every trap handler! Executing it elsewhere
 * or omitting it from a trap handler will cause undefined behavior and likely crash
 * the system when RETT tries to restore context.
 *
 * Notes:
 * - ENTT and RETT form a matched pair
 * - Always paired with InvokeTrapHandler() system call
 * - OTE clearing is critical for handler safety
 * - Saved OTE is restored by RETT
 * - Context save is atomic (cannot be interrupted)
 * - PCB structure varies by domain
 *
 * IMPLEMENTATION STATUS: STUB (Requires full trap system infrastructure)
 *
 * Current implementation:
 * - Validates operand count
 * - Logs trap handler entry
 * - Full context save REQUIRES:
 *   * PCB (Process Control Block) per domain
 *   * DomainContext structure
 *   * Trap enable registers (OTE1, OTE2, MTE1, MTE2)
 *   * Domain management infrastructure
 *   * Pending trap state tracking
 *
 * To implement ENTT fully:
 * 1. Implement PCB structure and management
 * 2. Implement domain system (CAD, mother domain, etc.)
 * 3. Implement trap enable registers (OTE1, OTE2, MTE1, MTE2)
 * 4. Implement DomainContext save/restore
 * 5. Implement InvokeTrapHandler() system call
 * 6. Integrate with trap dispatch mechanism
 *
 * Related Instructions:
 * - RETT: Return from trap handler (restores context)
 * - TRAP: Explicit software trap
 * - WAIT: Wait for interrupt
 *
 * Reference: ND-500 Reference Manual, Chapter 6.4
 *            COMPLETE_TRAP_SYSTEM_IMPLEMENTATION_ROADMAP.md Feature 4
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Entt.cs
 */
void nd500_instr_Entt(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* ========================================================================
     * STEP 1: VALIDATE OPERAND COUNT
     * ======================================================================== */
    if (fi->operand_count != 2) {
        printf("[ERROR] ENTT expects 2 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* ========================================================================
     * ENTT - Enter Trap Handler
     * ========================================================================
     *
     * ENTT is the first instruction in every trap handler routine.
     * It sets up the trap handler's local data field and saves the complete
     * register block so RETT can restore it later.
     *
     * Trap Handler Data Field Layout (at THA + 256):
     *   B+0    PREVB    Previous base (set to 0 for trap frame bottom)
     *   B+4    RETA     Return address (set to 0 for trap frame)
     *   B+8    SP       Stack pointer (B + stack demand)
     *   B+12   AUX      Auxiliary (protect violation info)
     *   B+16   N        Argument count = 50 (decimal)
     *   B+20   arg1     Trapping P (PC of trapped instruction)
     *   B+24   arg2     P register (return PC)
     *   B+28   arg3     L register
     *   B+32   arg4     B register (pre-trap value)
     *   B+36   arg5     R register
     *   B+40   arg6     I1
     *   B+44   arg7     I2
     *   B+48   arg8     I3
     *   B+52   arg9     I4
     *   B+56   arg10    A1
     *   B+60   arg11    A2
     *   B+64   arg12    A3
     *   B+68   arg13    A4
     *   B+72   arg14    E1
     *   B+76   arg15    E2
     *   B+80   arg16    E3
     *   B+84   arg17    E4
     *   B+88   arg18    ST1
     *   B+92   arg19    ST2
     *   B+96   arg20    PS
     *   B+100  arg21    TOS
     *   B+104  arg22    LL
     *   B+108  arg23    HL
     *   B+112  arg24    THA
     *   B+116  arg25    CED
     *   B+120  arg26    CAD
     *   B+124  arg27-30 mic (microcode scratch) - 4 words
     *   B+140  arg31    OTE1
     *   B+144  arg32    OTE2
     *   B+148  arg33    CTE1
     *   B+152  arg34    CTE2
     *   B+156  arg35    MTE1
     *   B+160  arg36    MTE2
     *   B+164  arg37    TEMM1
     *   B+168  arg38    TEMM2
     *   B+172  arg39-40 mic (microcode scratch) - 2 words
     *   B+180+ Local data area for handler
     * ======================================================================== */

    /* Verify a trap dispatch is awaiting its ENTT (set by invoke_trap_handler).
     * Deliberately NOT in_trap_handler: with nested traps allowed, a deeper
     * handler's ENTT runs while the outer handler is still active, so
     * in_trap_handler stays set across levels and cannot mark "this dispatch
     * has not been consumed yet". See cpu_protos.h trap_dispatch_pending. */
    if (!cpu->trap_dispatch_pending) {
        printf("[ERROR] ENTT at PC=0x%08X: Not in trap handler context\n",
               fi->address);
        trap_instruction_sequence_error(cpu, fi->address);
        return;
    }

    /* Read operands */
    /* Operand 0: Local data field size (in bytes or halfwords) */
    /* Operand 1: Stack demand (total trap handler stack demand) */
    uint32_t local_data_size = read_operand_w(cpu, &fi->operands[0]);
    uint32_t stack_demand = read_operand_w(cpu, &fi->operands[1]);

    /* Save pre-trap register values before we modify B */
    uint32_t saved_B = cpu->B;
    uint32_t saved_L = cpu->L;
    uint32_t saved_TOS = cpu->TOS;

    /* Calculate trap handler local data field address: THA + 256 bytes */
    /* The THA register points to the start address vector (64 words = 256 bytes) */
    /* The local data field follows immediately after the vector table */
    uint32_t trap_frame_base = cpu->THA + 256;

    /* Save the pending CALL/ENT* sequence-interlock state under this frame's
     * address and clear the live fields, so the handler starts with a clean
     * interlock and the trapped code resumes with its own. The matching RETT
     * pops using the same key. Without this a page fault taken on a callee's
     * entry instruction resumes with the interlock cleared by the handler's own
     * CALL/ENT* pairs, and the retried entry instruction raises a false ISE. */
    nd500_trap_seq_push(cpu, trap_frame_base);
    cpu->pending_call_return_address = 0;
    cpu->pending_call_arg_count = 0;

    /* ========================================================================
     * Set up new B register to point to trap handler local data field
     * ======================================================================== */
    cpu->B = trap_frame_base;

    /* ========================================================================
     * Write trap handler data field header (5 words at B+0..B+19)
     * ======================================================================== */

    /* B+0: PREVB = 0 (marks bottom of trap handler stack) */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 0, 1, 0), 0);

    /* B+4: RETA = 0 (no return address for trap frame) */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 4, 1, 0), 0);

    /* B+8: SP = B + local_data_size (stack pointer for handler's local data area) */
    /* Per ND-500 manual Step 2: B.SP := B + operand1 (main program stack demand) */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 8, 1, 0),
                      trap_frame_base + local_data_size);

    /* Trap-frame heading fields B+12 (AUX) and B+16 (N).
     *
     * On the NDIX multi-domain (mother-domain) trap path the ND-500 microcode
     * overloads these heading slots with the fault info and the FAULTING LOGICAL
     * ADDRESS: entrap (machine/locore.c:461-462) does
     *     w move b.12, b.24+CX_INFO      # fault info
     *     w move b.16, b.24+CX_VADDR     # fault address
     * and pagein() faults the address in. Writing the literal arg-count 50 at B+16
     * made the kernel page in address 0x32 (=50) forever (panic: pagein valid page).
     *
     * SINTRAN's own trap handling instead reads B+16 as the register-block arg
     * count (N=50) and hangs / errors (NC prints [SINTRAN ERROR 132B]) if it sees a
     * fault address there. NDIX is the guest-tables MMU regime (ND500X_MMU_GUEST_
     * TABLES=1, the same gate the rest of the NDIX-specific MMU code uses); SINTRAN
     * (incl. NC codegen) never sets it. So deliver the fault address/info only under
     * guest-tables mode and keep the classic N=50 heading otherwise. (trap_cross_
     * domain alone is NOT sufficient: NC also takes cross-domain traps.) */
    static int ndix_regime = -1;
    if (ndix_regime < 0) {
        const char* e = getenv("ND500X_MMU_GUEST_TABLES");
        ndix_regime = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    if (ndix_regime) {
        nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 12, 1, 0),
                          cpu->trap_saved_info);
        nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 16, 1, 0),
                          cpu->trap_saved_fault_addr);
    } else {
        /* B+12: AUX = 0 ; B+16: N = 50 (argument count - register block layout). */
        nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 12, 1, 0), 0);
        nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 16, 1, 0), 50);
    }

    /* ========================================================================
     * Write register block (args 1-40 at B+20..B+179)
     * ======================================================================== */

    /* arg1 (B+20): Trapping P - PC of instruction that caused trap */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 20, 1, 0),
                      cpu->trap_saved_PC);

    /* arg2 (B+24): P register - the resume address RETT returns to. Equals the
     * trapping P for Before/During-class traps (retry) but the NEXT instruction
     * for After-class traps (manual ND-05.009.4 page 79 + Table 10). */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 24, 1, 0),
                      cpu->trap_resume_PC);

    /* arg3 (B+28): L register (link/return address) - CRITICAL for subroutine returns */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 28, 1, 0), saved_L);

    /* arg4 (B+32): B register (pre-trap base) */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 32, 1, 0), saved_B);

    /* arg5 (B+36): R register */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 36, 1, 0), cpu->R);

    /* arg6-9 (B+40..B+52): I1-I4 */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 40, 1, 0), cpu->I[0]);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 44, 1, 0), cpu->I[1]);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 48, 1, 0), cpu->I[2]);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 52, 1, 0), cpu->I[3]);

    /* arg10-13 (B+56..B+68): A1-A4 */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 56, 1, 0), cpu->A[0]);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 60, 1, 0), cpu->A[1]);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 64, 1, 0), cpu->A[2]);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 68, 1, 0), cpu->A[3]);

    /* arg14-17 (B+72..B+84): E1-E4 */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 72, 1, 0), cpu->E[0]);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 76, 1, 0), cpu->E[1]);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 80, 1, 0), cpu->E[2]);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 84, 1, 0), cpu->E[3]);

    /* arg18-19 (B+88..B+92): ST1, ST2 */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 88, 1, 0), cpu->ST1);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 92, 1, 0), cpu->ST2);

    /* arg20 (B+96): PS */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 96, 1, 0), cpu->PS);

    /* arg21-23 (B+100..B+108): TOS, LL, HL */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 100, 1, 0), saved_TOS);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 104, 1, 0), cpu->LL);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 108, 1, 0), cpu->HL);

    /* arg24 (B+112): THA */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 112, 1, 0), cpu->THA);

    /* arg25-26 (B+116..B+120): CED, CAD.
     * For a cross-domain (mother-domain) trap, raise_trap already switched the
     * live CED/CAD to the HANDLER domain, but the register block must record the
     * TRAPPING domain's CED/CAD so RETT returns control to the trapping domain
     * (manual 4.2.5.3). raise_trap stashed those in trap_saved_CED/CAD. For a
     * same-domain trap trap_cross_domain==0 and these equal the live values. */
    uint32_t entt_ced = cpu->trap_cross_domain ? cpu->trap_saved_CED : cpu->CED;
    uint32_t entt_cad = cpu->trap_cross_domain ? cpu->trap_saved_CAD : cpu->CAD;
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 116, 1, 0), entt_ced);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 120, 1, 0), entt_cad);

    /* arg27-30 (B+124..B+136): mic scratch - set to 0 */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 124, 1, 0), 0);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 128, 1, 0), 0);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 132, 1, 0), 0);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 136, 1, 0), 0);

    /* arg31-32 (B+140..B+144): OTE1, OTE2 (saved values from invoke_trap_handler) */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 140, 1, 0),
                      cpu->trap_saved_OTE1);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 144, 1, 0),
                      cpu->trap_saved_OTE2);

    /* arg33-34 (B+148..B+152): CTE1, CTE2 */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 148, 1, 0), cpu->CTE1);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 152, 1, 0), cpu->CTE2);

    /* arg35-36 (B+156..B+160): MTE1, MTE2 */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 156, 1, 0), cpu->MTE1);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 160, 1, 0), cpu->MTE2);

    /* arg37-38 (B+164..B+168): TEMM1, TEMM2 */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 164, 1, 0), cpu->TEMM1);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 168, 1, 0), cpu->TEMM2);

    /* arg39-40 (B+172..B+176): mic scratch - set to 0 */
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 172, 1, 0), 0);
    nd500_bus_write32(cpu->machine, nd500_mmu_translate(cpu, trap_frame_base + 176, 1, 0), 0);

    /* ========================================================================
     * Set up trap handler stack registers (per ND-500 manual Steps 5-6)
     * ======================================================================== */

    /* Step 5: TOS := B + operand2 (total trap handler stack demand) */
    cpu->TOS = trap_frame_base + stack_demand;

    /* Step 6: L := B.SP (L points to first free location, same as B.SP) */
    cpu->L = trap_frame_base + local_data_size;

    /* Note: LL and HL are NOT modified by ENTT - the pre-trap values are saved
     * in the register block but the current LL/HL remain unchanged */

    /* ========================================================================
     * Step 9: Clear the specific trap status bit before handler execution
     * ======================================================================== */
    uint64_t trapBit = 1ULL << cpu->trap_number;
    if (cpu->trap_number < 32) {
        cpu->ST1 &= ~(uint32_t)(trapBit & 0xFFFFFFFF);
    } else {
        cpu->ST2 &= ~(uint32_t)(trapBit >> 32);
    }

    /* ========================================================================
     * Step 4: Copy 10 words of program memory for diagnostics (arg41-50)
     * Note: This is diagnostic data only. We skip it for now because:
     * - The MMU translate for program memory may trigger page faults
     * - Since we're inside a trap handler (OTE=0), page faults would be fatal
     * - The SINTRAN trap handler doesn't appear to use this data
     * TODO: Implement safe read that doesn't raise traps on page fault
     * ======================================================================== */
    /* Skip program memory copy for now - just zero the area */
    for (int i = 0; i < 10; i++) {
        nd500_bus_write32(cpu->machine,
            nd500_mmu_translate(cpu, trap_frame_base + 180 + i * 4, 1, 0), 0);
    }

    /* The trap context is now entirely in the guest-memory frame at THA, so the
     * emulator's single-level trap_saved_* fields are free to be reused. Release
     * the nesting interlock: from here on a fault inside this handler can be
     * dispatched normally (NDIX expects that - psig() touches the _Udata window
     * and legitimately page-faults), and RETT rebuilds state from this frame. */
    cpu->trap_dispatch_pending = 0;

    /* PC continues to next instruction (trap handler body) */
}
