#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdlib.h>   /* getenv - implicit-int prototype truncates the char* -> crash */

/* Once-latched env flag: getenv() on the CPU run path races readline's
 * setenv (environ realloc) on the main thread -> SIGSEGV. Latch once. */
static int nd_env_flag(const char* name, int* latch) {
    if (*latch < 0) { const char* e = getenv(name); *latch = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return *latch;
}
static int g_envf_domdbg = -1;


/**
 * RETK instruction - CALL class
 *
 * Mnemonic: retk
 * Operands: 0
 * Opcode: 0x0081
 *
 * Operation: Return from subroutine (set K flag return)
 *
 * Description:
 * Returns from a subroutine entered through ENTS, ENTSN, ENTF, ENTFN, ENTM, ENTD, or ENTB.
 * Identical to RET except SETS the K flag instead of clearing it.
 *
 * This instruction is used when a subroutine wants to signal an error or special
 * condition to the caller by returning with K set.
 *
 * Operand Structure:
 * - No operands (register-only operation)
 *
 * Operation Steps:
 * 1. SET K flag (KEY/Invalid flag) - only difference from RET
 * 2. Read PREVB from B+0 (previous B register value)
 * 3. Read RETA from B+4 (return address)
 * 4. Check for stack underflow (PREVB == 0 without domain boundary)
 * 5. If PREVB == 0 or RETA == 0:
 *    a. Check for domain boundary (CAD != CED)
 *    b. If domain boundary: perform cross-domain return (NOT IMPLEMENTED)
 *    c. If no domain to return to: raise stack underflow trap
 * 6. Otherwise (normal return):
 *    a. Set P = RETA (jump to return address)
 *    b. Set L = RETA (save return address in L)
 *    c. Set B = PREVB (restore previous stack frame)
 *
 * Flag Behavior:
 * - K (Key/Invalid): SET to 1 (error condition signaled to caller)
 * - Z, S, C, O: Unaffected
 *
 * Trap Conditions:
 * - Address trap fetch (ATF) if return address is invalid
 * - Stack underflow (STU) if PREVB == 0 (no previous frame)
 *
 * Typical Usage:
 *   ; Return with error condition signaled
 *   SUB1:  ENTS #100     ; Create stack frame
 *          ...
 *          IF (error) THEN
 *            RETK        ; Return with K flag set (error)
 *          ELSE
 *            RET         ; Return with K flag clear (success)
 *          FI
 *
 * Notes:
 * - Must be paired with ENT* instruction (ENTS, ENTF, etc.)
 * - RET clears K flag, RETK sets K flag before returning
 * - K flag is typically used to signal errors or special conditions
 *
 * Related Instructions:
 * - RET: Return and clear K flag
 * - RETK: Return and set K flag [this instruction]
 * - RETB: Return from buddy subroutine (free heap block)
 * - RETBK: Return from buddy subroutine and set K flag
 *
 * Reference: ND-500 Reference Manual, Chapter 13.11
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Retk.cs
 */
void nd500_instr_Retk(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Stack frame field offsets */
    const uint32_t OFFSET_PREVB = 0;   /* Previous B */
    const uint32_t OFFSET_RETA  = 4;   /* Return address */

    /* Validate operand count */
    if (fi->operand_count != 0) {
        printf("[ERROR] RETK expects 0 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* STEP 1: SET K flag (RETK sets K, RET clears it) */
    nd500_set_flag(cpu, ND500_FLAG_K);

    /* STEP 2: Read PREVB from B+0 (previous B register value) */
    uint32_t prev_b = nd500_read_memory_32(cpu, cpu->B + OFFSET_PREVB);

    /* STEP 3: Read RETA from B+4 (return address) */
    uint32_t ret_addr = nd500_read_memory_32(cpu, cpu->B + OFFSET_RETA);

    /* STEP 4: Check for domain boundary (PREVB == 0 or RETA == 0). Same cross-domain
     * return as Ret.c (RET), but the K flag stays SET (set at STEP 1) so the returned-
     * to program sees the syscall's error indication. This is the NDIX syscall error
     * return path: _domain_call ends `if k go k_is1: ret ; k_is1: retk` (locore.c:422),
     * so a syscall that set K returns to the user via RETK. The caller's CED/CAD/P/B
     * are loaded from the OLD (=CED) domain's DIT call area, which the kernel's
     * _domain_call restored to the (execve-modified) user P/B. */
    if ((prev_b == 0 || ret_addr == 0) && (cpu->CAD != cpu->CED) && (cpu->CAD != 0)) {
        uint32_t old_base = cpu->DITBASE + (uint32_t)cpu->CED * 256u;
        uint8_t  new_ced = nd500_bus_read8(cpu->machine, old_base + 128);   /* call_ce */
        uint8_t  new_cad = nd500_bus_read8(cpu->machine, old_base + 129);   /* call_ca */
        uint32_t new_p   = nd500_bus_read32(cpu->machine, old_base + 131);  /* call_p  */
        uint32_t new_b   = nd500_bus_read32(cpu->machine, old_base + 135);  /* call_b  */

        cpu->CED = new_ced;
        cpu->CAD = new_cad;
        cpu->PC  = new_p;
        cpu->L   = new_p;
        cpu->B   = new_b;
        nd500_apply_domain_pia(cpu, new_ced);

        /* Zeroize call_p/call_b in the old DIT so the domain may be called again. */
        nd500_bus_write32(cpu->machine, old_base + 131, 0);
        nd500_bus_write32(cpu->machine, old_base + 135, 0);

        if (nd_env_flag("ND500X_DOMDBG", &g_envf_domdbg))
            /* I1 carries the syscall error code on a K-set return (libc
             * _syscall: "if k go cerror"; cerror stores W1 into _errno). */
            printf("[DOMRETK] RETK@0x%08X: domain %u -> %u  P=0x%08X B=0x%08X CAD=%u (K set) errno(I1)=%u\n",
                   fi->address, (unsigned)new_cad, new_ced, new_p, new_b, new_cad,
                   cpu->I[0]);
        return;
    }

    /* STEP 5: Check for STACK UNDERFLOW (after ruling out domain boundary) */
    if (prev_b == 0) {
        printf("[TRAP] RETK at PC=0x%08X: Stack underflow (PREVB=0, no domain to return to)\n",
               fi->address);
        trap_stack_underflow(cpu, fi->address);
        return;
    }

    /* STEP 6: Normal return - restore stack frame */
    cpu->PC = ret_addr;      /* Jump to return address (B.RETA → P) */
    cpu->L = ret_addr;       /* Update link register (B.RETA → L) */
    cpu->B = prev_b;         /* Restore previous stack frame (B.PREVB → B) */

    /* K flag already set, all other flags unaffected */
}
