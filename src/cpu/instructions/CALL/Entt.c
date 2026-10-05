/*
 * Entt.c - ND-500 ENTT instruction (CALL class)
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
#include "nd500_mmu.h"
#include <stdio.h>
#include <stdlib.h>
#include "nd500_settings.h"   /* emulator knobs, as plain fields */
/* The <stdlib.h> above used to carry a warning that getenv()'s implicit int
 * prototype truncated the returned char* to 32 bits and made the env check
 * deref a wild pointer (a real SIGSEGV in ENTT). That whole class of bug is
 * gone: this file reads a struct field now, not the environment. */

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
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Entt.cs
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
    /* THE GUARD IS THE INSIDE-TRAP-HANDLER FLAG, AS IN THE REFERENCE.
     *
     * RetroCore's Entt.cs tests `pcb.InsideTrapHandler` for regs.CAD - the PCB
     * flag, which is the DIT's own byte at offset 187 - and not a
     * dispatch-to-ENTT interlock. nd500x tested trap_dispatch_pending, which a
     * park between the dispatch and the handler's first instruction legitimately
     * spends: measured 503 instruction-sequence traps at the handler entry
     * 0x08004924 on PLACE-DOMAIN CPU-STAT, with the interlock reading 0 at every
     * context save. nd500_is_in_trap_handler reads the DIT when one is
     * configured and the CPU field otherwise, so a machine with no DIT (the
     * conformance corpus) behaves as before. */
    if (!nd500_is_in_trap_handler(cpu)) {
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

    /* EVERY FRAME PAGE FIRST, BEFORE ANY REGISTER IS TOUCHED - AND THE FAULT
     * NAMES THE ORIGINAL TRAPPING INSTRUCTION, NOT THIS ENTT.
     *
     * Two separate things had to be right here, and the second is the reference's,
     * not a guess. nd500x used to set B to the frame base and push the sequence
     * interlock BEFORE writing the frame, so a frame write that faulted left the
     * instruction half applied.
     *
     * MEASURED 05-OCT-2026 on PLACE-DOMAIN CPU-STAT, per-step trace:
     *   rt P=0x08004924 BC CE B=0x00000004 L=0x08000018   <- first attempt
     *   [MMU] PS_ASI page not valid! vaddr=0x08001800
     *   trap 46B at P=0x08004924 addr=0x08001800 - parked, SINTRAN pages it in
     *   rt P=0x08004924 BC CE B=0x08001728 L=0x08001804   <- retry, B ALREADY SET
     *   raise_trap trapBit=0x800000000 (instruction sequence error)
     * and from there the handler's RETT was refused 503 times.
     *
     * RESTARTING AT THE ENTT IS NOT THE DESIGN, and reporting the fault at this
     * instruction's own address would do exactly that. RetroCore settled it on
     * the microword lane (EnttFaultRestartTests, Prefetch500.FaultRestartStart):
     * the handler dispatch 011622B-011630B never writes P and ENTT moves P only
     * at its last word 011735B (AD,PC), so on the hardware P still names the
     * ORIGINAL trapping instruction while ENTT runs. A fault inside ENTT
     * restarts THAT instruction, the trap is raised again, and ENTT is re-entered
     * from the top with the page present. Their note records that restart-at-ENTT
     * was tried and produced an instruction sequence error, because every process
     * continue runs 011370B and the restarted ENTT then takes its ISE arm - the
     * same ISE measured here.
     *
     * cpu->trap_saved_PC is that instruction (cpu.c sets it to the trapping P on
     * dispatch); fall back to this address only if no dispatch is on record. */
    {
        const uint32_t frame_span = 180u + 40u;
        uint32_t fault_pc = (cpu->trap_saved_PC != 0u) ? cpu->trap_saved_PC : fi->address;
        for (uint32_t off = 0; off <= frame_span; off += 4u) {
            if (nd500_mmu_peek_space(cpu, trap_frame_base + off, (uint8_t)cpu->CED, 0)
                == 0xFFFFFFFFu) {
                cpu->PC = fault_pc;
                trap_page_fault(cpu, fault_pc, trap_frame_base + off);
                return;
            }
        }
    }

    /* Save the pending CALL/ENT* sequence-interlock state under this frame's
     * address and clear the live fields, so the handler starts with a clean
     * interlock and the trapped code resumes with its own. The matching RETT
     * pops using the same key. Without this a page fault taken on a callee's
     * entry instruction resumes with the interlock cleared by the handler's own
     * CALL/ENT* pairs, and the retried entry instruction raises a false ISE. */
    nd500_trap_seq_push(cpu, trap_frame_base);
    cpu->pending_call_return_address = 0;
    cpu->pending_call_arg_count = 0;

    /* B, TOS, L AND THE TRAP ENABLES ARE SET AT THE END, NOT HERE.
     *
     * RetroCore's Entt.cs writes the whole frame first, checks
     * cpu.InstructionAborted, and only then assigns regs.B, regs.TOS, regs.L
     * and clears OTE1/OTE2 - so a frame write that faults leaves no register
     * changed and the instruction is retryable. nd500x set B here, before the
     * writes, and the retry after a pagein then ran with B already pointing at
     * the frame: measured as "rt P=0x08004924 BC CE B=0x08001728" on the second
     * attempt where the first had B=4, followed by an instruction sequence
     * error. Every other ENT* variant in this directory already follows the
     * reference's order; this one did not. */

    /* ========================================================================
     * Write trap handler data field header (5 words at B+0..B+19)
     * ======================================================================== */

    /* B+0: PREVB = 0 (marks bottom of trap handler stack) */
    nd500_write_memory_32(cpu, trap_frame_base + 0, 0);

    /* B+4: RETA = 0 (no return address for trap frame) */
    nd500_write_memory_32(cpu, trap_frame_base + 4, 0);

    /* B+8: SP = B + local_data_size (stack pointer for handler's local data area) */
    /* Per ND-500 manual Step 2: B.SP := B + operand1 (main program stack demand) */
    nd500_write_memory_32(cpu, trap_frame_base + 8,
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
    if (ndix_regime < 0) ndix_regime = nd500_settings()->mmu_guest_tables;
    if (ndix_regime) {
        nd500_write_memory_32(cpu, trap_frame_base + 12,
                          cpu->trap_saved_info);
        nd500_write_memory_32(cpu, trap_frame_base + 16,
                          cpu->trap_saved_fault_addr);
    } else {
        /* B+12: AUX = 0 ; B+16: N = 50 (argument count - register block layout). */
        nd500_write_memory_32(cpu, trap_frame_base + 12, 0);
        nd500_write_memory_32(cpu, trap_frame_base + 16, 50);
    }

    /* ========================================================================
     * Write register block (args 1-40 at B+20..B+179)
     * ======================================================================== */

    /* arg1 (B+20): Trapping P - PC of instruction that caused trap */
    nd500_write_memory_32(cpu, trap_frame_base + 20,
                      cpu->trap_saved_PC);

    /* arg2 (B+24): P register - the resume address RETT returns to. Equals the
     * trapping P for Before/During-class traps (retry) but the NEXT instruction
     * for After-class traps (manual ND-05.009.4 page 79 + Table 10). */
    nd500_write_memory_32(cpu, trap_frame_base + 24,
                      cpu->trap_resume_PC);

    /* arg3 (B+28): L register (link/return address) - CRITICAL for subroutine returns */
    nd500_write_memory_32(cpu, trap_frame_base + 28, saved_L);

    /* arg4 (B+32): B register (pre-trap base) */
    nd500_write_memory_32(cpu, trap_frame_base + 32, saved_B);

    /* arg5 (B+36): R register */
    nd500_write_memory_32(cpu, trap_frame_base + 36, cpu->R);

    /* arg6-9 (B+40..B+52): I1-I4 */
    nd500_write_memory_32(cpu, trap_frame_base + 40, cpu->I[0]);
    nd500_write_memory_32(cpu, trap_frame_base + 44, cpu->I[1]);
    nd500_write_memory_32(cpu, trap_frame_base + 48, cpu->I[2]);
    nd500_write_memory_32(cpu, trap_frame_base + 52, cpu->I[3]);

    /* arg10-13 (B+56..B+68): A1-A4 */
    nd500_write_memory_32(cpu, trap_frame_base + 56, cpu->A[0]);
    nd500_write_memory_32(cpu, trap_frame_base + 60, cpu->A[1]);
    nd500_write_memory_32(cpu, trap_frame_base + 64, cpu->A[2]);
    nd500_write_memory_32(cpu, trap_frame_base + 68, cpu->A[3]);

    /* arg14-17 (B+72..B+84): E1-E4 */
    nd500_write_memory_32(cpu, trap_frame_base + 72, cpu->E[0]);
    nd500_write_memory_32(cpu, trap_frame_base + 76, cpu->E[1]);
    nd500_write_memory_32(cpu, trap_frame_base + 80, cpu->E[2]);
    nd500_write_memory_32(cpu, trap_frame_base + 84, cpu->E[3]);

    /* arg18-19 (B+88..B+92): ST1, ST2 */
    nd500_write_memory_32(cpu, trap_frame_base + 88, cpu->ST1);
    nd500_write_memory_32(cpu, trap_frame_base + 92, cpu->ST2);

    /* arg20 (B+96): PS */
    nd500_write_memory_32(cpu, trap_frame_base + 96, cpu->PS);

    /* arg21-23 (B+100..B+108): TOS, LL, HL */
    nd500_write_memory_32(cpu, trap_frame_base + 100, saved_TOS);
    nd500_write_memory_32(cpu, trap_frame_base + 104, cpu->LL);
    nd500_write_memory_32(cpu, trap_frame_base + 108, cpu->HL);

    /* arg24 (B+112): THA */
    nd500_write_memory_32(cpu, trap_frame_base + 112, cpu->THA);

    /* arg25-26 (B+116..B+120): CED, CAD.
     * For a cross-domain (mother-domain) trap, raise_trap already switched the
     * live CED/CAD to the HANDLER domain, but the register block must record the
     * TRAPPING domain's CED/CAD so RETT returns control to the trapping domain
     * (manual 4.2.5.3). raise_trap stashed those in trap_saved_CED/CAD. For a
     * same-domain trap trap_cross_domain==0 and these equal the live values. */
    uint32_t entt_ced = cpu->trap_cross_domain ? cpu->trap_saved_CED : cpu->CED;
    uint32_t entt_cad = cpu->trap_cross_domain ? cpu->trap_saved_CAD : cpu->CAD;
    nd500_write_memory_32(cpu, trap_frame_base + 116, entt_ced);
    nd500_write_memory_32(cpu, trap_frame_base + 120, entt_cad);

    /* arg27-30 (B+124..B+136): mic scratch - set to 0 */
    nd500_write_memory_32(cpu, trap_frame_base + 124, 0);
    nd500_write_memory_32(cpu, trap_frame_base + 128, 0);
    nd500_write_memory_32(cpu, trap_frame_base + 132, 0);
    nd500_write_memory_32(cpu, trap_frame_base + 136, 0);

    /* arg31-32 (B+140..B+144): OTE1, OTE2 (saved values from invoke_trap_handler) */
    nd500_write_memory_32(cpu, trap_frame_base + 140,
                      cpu->trap_saved_OTE1);
    nd500_write_memory_32(cpu, trap_frame_base + 144,
                      cpu->trap_saved_OTE2);

    /* arg33-34 (B+148..B+152): CTE1, CTE2 */
    nd500_write_memory_32(cpu, trap_frame_base + 148, cpu->CTE1);
    nd500_write_memory_32(cpu, trap_frame_base + 152, cpu->CTE2);

    /* arg35-36 (B+156..B+160): MTE1, MTE2 */
    nd500_write_memory_32(cpu, trap_frame_base + 156, cpu->MTE1);
    nd500_write_memory_32(cpu, trap_frame_base + 160, cpu->MTE2);

    /* arg37-38 (B+164..B+168): TEMM1, TEMM2 */
    nd500_write_memory_32(cpu, trap_frame_base + 164, cpu->TEMM1);
    nd500_write_memory_32(cpu, trap_frame_base + 168, cpu->TEMM2);

    /* arg39-40 (B+172..B+176): mic scratch - set to 0 */
    nd500_write_memory_32(cpu, trap_frame_base + 172, 0);
    nd500_write_memory_32(cpu, trap_frame_base + 176, 0);

    /* ========================================================================
     * Set up trap handler stack registers (per ND-500 manual Steps 5-6)
     * ======================================================================== */


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
        nd500_write_memory_32(cpu, trap_frame_base + 180 + i * 4, 0);
    }

    /* NOW THE REGISTERS, AND NOT BEFORE. The reference's order exactly:
     * every frame write first, then one abort check, then B, TOS, L and the
     * trap enables (Entt.cs, the tail after its InstructionAborted check). A
     * fault anywhere above therefore leaves the instruction retryable. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }
    cpu->B = trap_frame_base;
    /* Step 5: TOS := B + operand2 (total trap handler stack demand) */
    cpu->TOS = trap_frame_base + stack_demand;
    /* Step 6: L := B.SP (L points to first free location, same as B.SP) */
    cpu->L = trap_frame_base + local_data_size;

    /* The trap context is now entirely in the guest-memory frame at THA, so the
     * emulator's single-level trap_saved_* fields are free to be reused. Release
     * the nesting interlock: from here on a fault inside this handler can be
     * dispatched normally (NDIX expects that - psig() touches the _Udata window
     * and legitimately page-faults), and RETT rebuilds state from this frame. */
    cpu->trap_dispatch_pending = 0;

    /* PC continues to next instruction (trap handler body) */
}
