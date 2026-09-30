/*
 * cpu_protos.h - CPU module prototypes and shared CPU types
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#ifndef CPU_PROTOS_H
#define CPU_PROTOS_H
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../machine/machine_types.h"

/* Per-instruction trap diagnostics.
 *
 * These were plain printf(), i.e. STDOUT - and under --ndix stdout IS the
 * NDIX console. A guest that overflows on a timer tick then interleaves its
 * own output with ours: "Connected to loca[TRAP] ADD2 ... lhost." Worse, it
 * is indistinguishable from guest output to anything parsing the console.
 *
 * These traps occur in normal operation - 4.3BSD C overflows signed arithmetic
 * routinely, and the ND-500 only *takes* an overflow trap when the trap is
 * enabled - so they are off by default. Set ND500X_TRAPLOG=1 to get them back,
 * on stderr where diagnostics belong. Trap behaviour itself is unaffected:
 * only the printing is gated. */
int nd500x_traplog(void);
#define ND500X_TRAPLOG(...)                                    \
    do {                                                       \
        if (nd500x_traplog()) fprintf(stderr, __VA_ARGS__);    \
    } while (0)

/* Maximum operands in decoded instruction struct.
 * Variable-operand instructions (CALL/CALLG/POLY) can have 2 fixed + 255 variable = 257 operands.
 * Using 258 to match C# MaxVariableOperands for consistency. */
#define ND500_MAX_OPERANDS 258

/* Forward declarations for Nd500Cpu dependencies */
typedef enum Nd500AddrMode {
    ND500_ADDR_UNKNOWN = 0,
    ND500_ADDR_CONSTANT_SHORT,
    ND500_ADDR_LOCAL_SHORT,
    ND500_ADDR_RECORD_SHORT,
    ND500_ADDR_LOCAL,
    ND500_ADDR_LOCAL_PI,
    ND500_ADDR_LOCAL_IND,
    ND500_ADDR_LOCAL_IND_PI,
    ND500_ADDR_RECORD,
    ND500_ADDR_PREINDEXED,
    ND500_ADDR_ABSOLUTE,
    ND500_ADDR_ABSOLUTE_PI,
    ND500_ADDR_CONSTANT,
    ND500_ADDR_REGISTER,
    ND500_ADDR_DESCRIPTOR,
    ND500_ADDR_ALTERNATIVE
} Nd500AddrMode;

typedef struct Nd500OperandDecoded {
    uint8_t address_code;
    uint8_t has_alt_prefix;
    uint8_t has_desc_prefix;
    uint8_t reg;
    Nd500AddrMode mode;
    uint8_t data_len;
    uint8_t data[8];
    uint32_t effective_address;  /* Computed effective address for memory operands */
    uint8_t bit_position;        /* Bit position (0-7) within byte for BIT type addressing */
} Nd500OperandDecoded;

typedef struct Nd500Cpu {
    /* Core registers */
    uint32_t PC;
    /* Integer */
    uint32_t I[4];
    /* Float accumulators and extensions */
    uint32_t A[4];
    uint32_t E[4];
    /* Addressing registers */
    uint32_t L; /* link */
    uint32_t B; /* base */
    uint32_t R; /* record */
    /* Special */
    uint32_t TOS, LL, HL, THA;
    /* Control */
    uint32_t OTE1, OTE2, CTE1, CTE2, MTE1, MTE2, TEMM1, TEMM2;
    /* Status registers (64-bit split into two 32-bit registers) */
    uint32_t ST1, ST2;  /* Status register (64-bit) */
    /* Flags (simplified status) */
    uint32_t FLAGS;
    /* MMU registers */
    uint32_t PSTP;      /* Physical Segment Table Pointer */

    /*
     * Optional per-regime policy deciding which (domain, segment) pairs translate
     * through the GUEST's DIT/PST rather than the emulator shadow tables, with the
     * regime's own enable flag folded in. NULL means the architectural default:
     * walk the guest tables whenever DITBASE and PSTP are both set.
     *
     * This exists so kernel-specific routing - which NDIX needs and SINTRAN does
     * not - lives with the boot code that can justify each segment, instead of as
     * a list of Unix segment numbers inside the CPU. Installed with
     * nd500_mmu_set_guest_table_policy().
     */
    int (*guest_table_policy)(void *ctx, uint8_t domain, int segment);
    void *guest_table_policy_ctx;
    uint32_t DITBASE;   /* Domain Information Table Base */

    /*
     * 1 once a DIT base has been DECLARED, whatever its value.
     *
     * ZERO IS A VALID DIT BASE, which is why this cannot be inferred from DITBASE
     * itself. RetroCore records the point directly: "the correct base here is 0, so
     * 'nothing learned' and 'the base is zero' are the same number", and its own
     * learner therefore guards on the number of trap-config writes seen rather than
     * on the base value. Testing DITBASE != 0 instead sent a guest that legitimately
     * uses base 0 down the emulator shadow path, where the tables are NULL - measured
     * 30-SEP-2026 as "[MMU] Tables not initialized! PST=(nil) PCB=(nil)" followed by
     * a fetch that was never translated.
     *
     * Set by nd500_mmu_declare_dit_base(). Ported from RetroCore's
     * CpuND500.Domain.cs DeclareDitBase / regs.DitConfigured.
     */
    int dit_configured;
    uint32_t CED;       /* Current Executing Domain */
    uint32_t CAD;       /* Current Alternative Domain */
    uint32_t PS;        /* Process Segment */

    /* Domain allocation tracking (like C# domainsInUse[]) */
    uint8_t domains_in_use[256];  /* 0=free, 1=allocated. Domain 0 always in use (kernel) */

    /* Symbol domain (for debugger symbol lookup - separate from CED/CAD) */
    uint8_t symbol_domain;

    /* CALL/ENT handshake state (internal CPU state not visible to programs) */
    /* Set by invoke_trap_handler, cleared by the handler's ENTT once it has
     * copied the trap context into the guest-memory trap frame.
     *
     * Nesting is ONLY unsafe in that window. ENTT writes 48 context fields to
     * the frame at THA and RETT reads them back, so once ENTT has run, level N's
     * state lives in guest memory and the emulator's single-level trap_saved_*
     * fields are free to be reused by a deeper trap. NDIX is built for exactly
     * this - it keeps per-level context blocks and computes
     * THA = _u + U_CXB0 + traplevel*496.
     *
     * Blocking dispatch for the WHOLE handler (the old use of in_trap_handler)
     * made any fault inside a handler a hard halt, which is what stopped PRT:
     * psig() legitimately touches the _Udata window and page-faults. */
    uint32_t trap_dispatch_pending;

    uint32_t pending_call_return_address;  /* Return address from CALL to pass to ENT */
    uint32_t pending_call_arg_count;       /* Number of arguments from CALL */
    uint32_t pending_call_arg_addresses[256]; /* Effective addresses of arguments */

    /* Trap handler state (for ENTT/RETT) */
    bool in_trap_handler;           /* True while executing trap handler */
    uint32_t trap_saved_PC;         /* Trapping P: address of the instruction that trapped (frame arg1) */
    uint32_t trap_resume_PC;        /* P to resume at after RETT (frame arg2): equals trap_saved_PC
                                     * for Before/During-class traps (retry), but the NEXT instruction
                                     * for After-class traps (manual ND-05.009.4 Table 10 + page 79) */
    uint32_t trap_saved_OTE1;       /* Saved OTE1 for restoration by RETT */
    uint32_t trap_saved_OTE2;       /* Saved OTE2 for restoration by RETT */
    int trap_number;                /* Current trap being handled (bit position) */

    /* Saved CALL/ENT* sequence-interlock state across a trap-and-restart.
     * The "an ENT* must be preceded by CALL/CALLG" precondition is a REAL hardware
     * interlock (microword C,SEQ / INVSEQ; MICRO-5800-B30 CALL @000646, CALLG @000652
     * set it; ENTD @000660 -> INS_SEQ_ERR @003141 reads IDU,STS). The sequence state
     * lives in the IDU status, which is part of the trap-saved context (ENTT1 @014042
     * carries C,SEQ). So when a CALL's callee ENT* page-faults, the pending-call state
     * must be SAVED here on dispatch and RESTORED by RETT - otherwise the kernel
     * handler's own CALL/ENT* pairs clear the naked pending_call_* fields and the
     * resumed ENTS sees pending_call_return_address == 0 and raises a FALSE ISE.
     *
     * ONE SLOT IS NOT ENOUGH. Traps nest: a page fault during a callee's ENT*
     * saves the interlock, and then the kernel's page-fault handler takes its own
     * page fault. With a single slot the inner dispatch overwrote it with the
     * already-cleared live value, so the outer RETT restored 0 and the resumed
     * ENT* raised a FALSE ISE anyway. MEASURED 2026-07-30: 14 such nested
     * discards in one ordinary NDIX boot (all trap 38, all CED 0); vi is simply
     * the first program demand-paged heavily enough to land the resume on an
     * actual ENT* and die with "Memory fault - core dumped".
     *
     * So each handler ENTT pushes its own entry, keyed by the trap frame address
     * (THA+256) it builds. The key is NOT the resume PC: the kernel rewrites that
     * (machine/trap.c:430 and :472 set ap->cx_p = &fuerror when pagein() fails).
     * A pop searches newest-first for its key; on a hit it restores that entry and
     * drops everything above it, which is what makes an ABANDONED frame harmless -
     * a process killed by SIGSEGV never returns from its handler, and a plain depth
     * counter would drift upward forever on exactly the failure this fixes. The
     * ring evicts oldest-first so the depth is bounded whatever the guest does.
     *
     * THE POP IS NOT (ONLY) IN RETT. NDIX never executes RETT for kernel traps -
     * machine/locore.c trapex returns with `lregbl $CNTXMASK,r3` (see the note in
     * SYSTEM/Lregbl.c), so an lregbl that reloads P while a handler is active IS
     * the trap return and pops the newest entry. MEASURED 2026-07-30 before this
     * was wired up: 132 pushes and 0 pops in one boot, ring permanently saturated,
     * every interlock lost - which is why vi died with "Memory fault - core dumped"
     * on a false ISE after a page fault on its own ENTS at 0x0000FC15.
     *
     * RetroCore CpuND500.Trap.cs carries the identical structure and policy -
     * these two must not diverge. */
#define TRAP_SEQ_RING   16      /* nesting depth kept; oldest evicted beyond it */
#define TRAP_SEQ_MAXARG 256     /* matches pending_call_arg_addresses */
    struct {
        uint32_t frame_base;    /* key: trap frame address (THA+256) ENTT built */
        uint32_t return_address;
        uint32_t arg_count;
        uint32_t arg_addresses[TRAP_SEQ_MAXARG];
    } trap_seq[TRAP_SEQ_RING];
    uint32_t trap_seq_head;     /* index one past the newest entry, mod TRAP_SEQ_RING */
    uint32_t trap_seq_count;    /* live entries, <= TRAP_SEQ_RING */
    /* Cross-domain (mother-domain) trap dispatch state. When a trap in a child
     * domain is handled by a mother domain (manual 4.2.5.3 / ch.6), raise_trap
     * switches live CED/CAD to the handler domain and stashes the TRAPPING
     * domain's CED/CAD here so ENTT saves them into the register block and RETT
     * returns to the trapping domain. trap_cross_domain=0 for same-domain traps
     * (unchanged single-domain SINTRAN behaviour). */
    uint32_t trap_saved_CED;        /* Trapping domain's CED (for ENTT reg-block arg25) */
    uint32_t trap_saved_CAD;        /* Trapping domain's CAD (for ENTT reg-block arg26) */
    int trap_cross_domain;          /* 1 if the current trap switched domains */
    /* Faulting logical address + info for the current trap. On a trap the ND-500
     * microcode places the faulting address in the ENTT frame heading N field
     * (B+16) and the fault info in AUX (B+12); NDIX's entrap copies them to
     * cx_vaddr/cx_info and pagein() uses cx_vaddr. Saved here by raise_trap so ENTT
     * can write them (previously ENTT wrote the literal arg count 50 = 0x32 at B+16,
     * so pagein faulted address 0x32 and never mapped the real page). Placed at the
     * struct tail so adding them does not shift any earlier field's offset. */
    uint32_t trap_saved_fault_addr;
    uint32_t trap_saved_info;
    /* MMU fault-location code (MMWHERE nibble, plus MMINST 0x40 for an I-channel
     * access) for the CURRENT fault, set by the MMU walk just before it calls
     * trap_page_fault() or trap_protect_violation(). Values are the MMW_* defines
     * in nd500_mmu.h. raise_trap copies it into trap_saved_info -> cx_info, which
     * the NDIX kernel branches on (machine/trap.c): the PGF handler services only
     * PFZ2, and both protect-violation handlers attempt pagein() only for
     * PVWVIOL with MMINST clear. For a page fault, 0 => default PFZ2. */
    uint32_t mmu_pgf_where;

    /* Per-generic-device interrupt priority, captured from the FE_IDEV command
     * packet (machine/if.h: every _idev_cpk variant begins with "short ipl") and
     * used when that device's completion interrupt is delivered. Indexed by
     * generic device number; 0 = never connected, use the default. Without it
     * every completion went out at IPL_DK - see fe_deliver's caller. */
    uint8_t fe_dev_ipl[16];

    /* Variable operand buffer for CALL/CALLG/POLY (all operands including fixed) */
    Nd500OperandDecoded extra_operands[ND500_MAX_OPERANDS];
    uint16_t extra_operand_count;

    /* ND-100 I/O Processor Bridge Configuration */
    uint32_t nd100_memory_offset;  /* Physical memory offset for ND-100 memory (default: 0x40000) */

    /* Instruction counter for TIME MON call (MON 11B) */
    uint64_t instruction_count;  /* Total instructions executed since startup */

    /* SOLO / TUTTI (manual ND-05.009.4 ch.16.1-16.2, section 6.5.4).
     *
     * SOLO sets PSD (ST1 bit 4) to make the instructions up to the next TUTTI
     * an indivisible sequence; TUTTI clears it. Held for too long, that is a
     * Disable process switch Timeout (DT, bit 30).
     *
     * The limit is "256 micro-cycles" on the ND-500/2, but the manual is
     * explicit that "In the ND-5000 implementation these are macroinstruction
     * cycles" (ch.16.1), and the ND-5000 is what this emulates - so the count
     * is of executed instructions, which is a thing this CPU actually has.
     *
     * Equally explicit, same paragraph: "In privilege mode there is no
     * limitation to the duration of a SOLO operation. Unprivileged users are
     * not allowed to run in SOLO for more than 256 cycles." The timeout is
     * therefore only ever armed for unprivileged code - which is why NDIX,
     * whose kernel sits in SOLO around context switches and interrupt entry
     * (machine/locore.c:508, :737), is not affected. */
    uint64_t solo_start_icount;  /* instruction_count when SOLO set PSD */

    /* Start PC of the instruction currently being executed (= old_pc in
     * cpu_step, set BEFORE the pre-execute PC advance). A page fault must
     * restart the FAULTING instruction, but cpu->PC is already advanced to
     * the next instruction during execute; raise_trap uses this for the
     * restartable-fault (PGF) saved PC so the instruction re-executes. */
    uint32_t cur_instr_pc;

    /* P1 - the TRAPPING P register: the address of the instruction that caused
     * the most recent trap.
     *
     * The ND-500/5000 keeps TWO program registers. P is the restart address and
     * runs AHEAD of the fault (the fetch advances it, and also decodes operands,
     * before the instruction executes). P1 holds the instruction that failed.
     *
     * This is how the machine is documented and operated, not an emulator
     * convenience. ND-05.017.01 "ND-5000 HARDWARE MAINTENANCE" chapter 6 STEP 2
     * has the engineer find a failing instruction like this:
     *
     *     N500: ATTACH-PROCESS 0
     *     N500: LOOK-AT-REGISTER P
     *     P  : XXXXXXXXXX
     *     P1 : XXXXXXXXXX:<Failing instruction>
     *
     * ND annotate P1 as the failing instruction; P is NOT. The whole of STEP 2
     * exists because the address printed in a trap report (which is P) does not
     * identify the instruction. Same pair as the context block's "Trapping P
     * register" / "Restart P register" (Appendix A.1, registers 0 and 1).
     *
     * Worked example, measured 2026-08-03: a SINTRAN swapper trap reported
     * "At program address: 1 10533B" (= P = 0o1000010533) while the instruction
     * that actually faulted was the RPHS at 0o1000010525 - three instructions
     * earlier. Recovering that by hand took days; reading P1 is immediate.
     *
     * Latched on EVERY trap in raise_trap, unlike the PGF/PV-only restart
     * correction below, so a handler always sees the trap it was entered for.
     * Mirrored from RetroCore Registers.cs / CpuND500.Trap.cs 2026-08-03. */
    uint32_t P1;

    /* Pending ND-100 front-end (fecall) completion interrupt. An async FE_READ/
     * FE_WRIT/FE_DCTL fills its response packet immediately but the kernel blocks
     * in biowait() until a completion INTERRUPT drives diintr()->iodone(). The
     * fecall handler sets these; cpu_step delivers the interrupt (vector to
     * _intvec) at the next safe boundary once the CPU drops below IPL_DK. */
    /* Pending front-end completion interrupts.
     *
     * This was ONE slot - a flag plus gen/sub/rpk. Any completion raised while
     * another was still waiting to be delivered simply overwrote it, and the
     * first one was never seen by the guest. That is not theoretical: it is
     * why an async terminal open (a hard-carrier line, io/mx.c:334) never woke
     * up. The open's completion was raised during a busy part of boot, a disk
     * or clock completion landed on top of it, and the driver slept for ever
     * in mxopen() waiting for a carrier report that had been thrown away.
     *
     * A small ring keeps them all. Eight is well clear of what is ever in
     * flight - completions are delivered at the next instruction boundary, so
     * the queue drains almost as fast as it fills. */
#define FE_INT_QUEUE_SIZE 8
    struct {
        uint32_t gen;   /* generic device (DISK=1) */
        uint32_t sub;   /* sub-device */
        uint32_t rpk;   /* response-packet ND-100 word address */
    } fe_int_q[FE_INT_QUEUE_SIZE];
    unsigned fe_int_head;   /* next slot to deliver */
    unsigned fe_int_count;  /* how many are queued */
    unsigned fe_int_lost;   /* completions dropped because the ring was full */

    /* Set by raise_trap when a trap fires MID-instruction; cleared by cpu_step
     * before each execute. Instruction implementations must check it after any
     * operand memory access and ABORT (no destination commit) when set: the
     * trap restarts the instruction, so committing a result computed from the
     * faulted (garbage) read corrupts restart state. (invoke_trap_handler
     * clears the global trap state synchronously, so nd500_trap_occurred() is
     * already 0 back in the instruction - this flag is the reliable signal.) */
    uint32_t instr_aborted;

    /* 1 while nd500_execute_decoded runs the current instruction; 0 during
     * instruction fetch/decode. Lets raise_trap tell an instruction-FETCH
     * page fault (restart at the fetch address, since cur_instr_pc still
     * names the previous instruction) from a mid-EXECUTE program-space read
     * fault such as CALL's entry-point check (restart at the instruction
     * itself, or the restarted CALL resumes at its TARGET and the ENTS
     * faults with "no preceding CALL"). */
    uint32_t in_execute;

    Nd500Machine* machine;

    /* This CPU's own translation cache (src/cpu/nd500_tlb.h).
     *
     * A POINTER, not an embedded struct: Nd500Tlb is about 48KB and several
     * tests declare `Nd500Cpu cpu;` on the stack. Allocated and registered by
     * nd500_cpu_init(), released by nd500_cpu_free(). NULL means "no cache" and
     * every user checks, so a CPU that was never initialised still translates -
     * it just walks every access. */
    struct Nd500Tlb* tlb;

    /* This CPU's MMU state: the PST, the PCB table and the two I&D enable
     * flags (src/cpu/nd500_mmu.h). Allocated by nd500_cpu_init(), released by
     * nd500_cpu_free(). */
    struct Nd500MmuState* mmu;
} Nd500Cpu;

/* ND-500 Trap System Definitions */
/* Based on ND-500 Reference Manual Chapter 6 - THE TRAP SYSTEM */

/* Non-ignorable trap bits (will trigger longjmp) */
#define TRAP_XSE  (1ULL << 32)  /* Index Scaling Error */
#define TRAP_IIC  (1ULL << 33)  /* Illegal Instruction Code */
#define TRAP_IOS  (1ULL << 34)  /* Illegal Operand Specifier */
#define TRAP_ISE  (1ULL << 35)  /* Instruction Sequence Error */
#define TRAP_PV   (1ULL << 36)  /* Protect Violation */

/* Fatal trap bits (always trigger longjmp) */
#define TRAP_THM  (1ULL << 37)  /* Trap Handler Missing */
#define TRAP_PGF  (1ULL << 38)  /* Page Fault */
#define TRAP_PWF  (1ULL << 39)  /* Power Failure */
#define TRAP_PRF  (1ULL << 40)  /* Processor Fault */
#define TRAP_HF   (1ULL << 41)  /* Hardware Fault */

/* Mask for traps that interrupt instruction execution.
 *
 * Bits 32-41 only. DT(30) and DE(31) are non-ignorable too, but they are NOT
 * listed here on purpose: several call sites use this mask to mean "a fault
 * that aborts the current memory access" (see the FUWDBG and kernel-B checks in
 * cpu.c), and the process-switch traps are not that. TRAP_NONIGNORABLE_MASK
 * below is the one to test when the question is which DISPATCH PATH a trap
 * takes. */
#define TRAP_INTERRUPT_MASK (TRAP_XSE | TRAP_IIC | TRAP_IOS | \
                             TRAP_ISE | TRAP_PV | TRAP_THM | \
                             TRAP_PGF | TRAP_PWF | TRAP_PRF | TRAP_HF)

/* Ignorable traps (bits 11-29) - set status but don't longjmp */
/* Integer overflow. Bit 9, per ND-05.009.4 Table 7 "Data status bits" (O is
 * bit 9) and the linker manual's SET-TRAP-CONDITION list, which opens with
 * "9D OVERFLOW".
 *
 * Every integer instruction used to raise TRAP_IVO here instead - bit 11,
 * Invalid Operation - which is a different condition entirely, and one NDIX
 * DOES arm (machine/trap.h T_CMTE1 = 0xf413d800 has bit 11 set and bit 9
 * clear). So an ordinary signed overflow, which 4.3BSD C does routinely and
 * which NDIX deliberately ignores, was being delivered to the guest as an
 * Invalid Operation trap. Seen once per timer tick under ping/telnet. */
#define TRAP_O    (1ULL << 9)   /* Integer Overflow */
#define TRAP_IVO  (1ULL << 11)  /* Invalid Operation */
#define TRAP_DZ   (1ULL << 12)  /* Divide by Zero */
#define TRAP_FU   (1ULL << 13)  /* Floating Underflow */
#define TRAP_FO   (1ULL << 14)  /* Floating Overflow */
#define TRAP_BO   (1ULL << 15)  /* BCD Overflow */
#define TRAP_IOV  (1ULL << 16)  /* Illegal Operand Value */
#define TRAP_SIT  (1ULL << 17)  /* Single Instruction Trap */
#define TRAP_BT   (1ULL << 18)  /* Branch Trap */
#define TRAP_CT   (1ULL << 19)  /* Call Trap */
#define TRAP_BPT  (1ULL << 20)  /* Breakpoint Trap */
#define TRAP_ATF  (1ULL << 21)  /* Address Trap Fetch */
#define TRAP_ATR  (1ULL << 22)  /* Address Trap Read */
#define TRAP_ATW  (1ULL << 23)  /* Address Trap Write */
#define TRAP_AZ   (1ULL << 24)  /* Address Zero Access */
#define TRAP_DR   (1ULL << 25)  /* Descriptor Range */
#define TRAP_IX   (1ULL << 26)  /* Illegal Index */
#define TRAP_STO  (1ULL << 27)  /* Stack Overflow */
#define TRAP_STU  (1ULL << 28)  /* Stack Underflow */
#define TRAP_PRT  (1ULL << 29)  /* Programmed Trap */
#define TRAP_DT   (1ULL << 30)  /* Disable process switch Timeout */
#define TRAP_DE   (1ULL << 31)  /* Disable process switch Error */

/* Mask for ignorable traps only: bits 9 and 11-29. Bit 10 is undefined.
 *
 * This used to be 0x3FFFF800 - bits 11-29 - which left OVERFLOW(9) out, so the
 * overflow trap could never be dispatched no matter who enabled it. Eighteen
 * instructions set the O status bit and not one of them could ever reach a
 * handler.
 *
 * Bit 9 belongs here on two independent authorities:
 *   - ND-05.009.4 ND-500 Reference Manual, Table 7 "Data status bits": O is
 *     bit 9, and "The Z, C, and S status bits have no corresponding trap
 *     conditions... All other data status bits are ignorable trap conditions."
 *   - ND-860289-2 Linker manual, SET-TRAP-CONDITION: the ignorable list opens
 *     with "9D OVERFLOW" and the non-ignorable list starts at 30D.
 * docs/ND-500-TRAPS.md in this repo already said 9-29; only the code disagreed.
 * TRAP_AFTER_MASK below has always included bit 9, so the two were out of step.
 *
 * NDIX is unaffected either way: it arms neither bit 9 nor anything else below
 * 11 (machine/trap.h T_CMTE1 = 0xf413d800), and a child cannot add bits of its
 * own (T_CTEMM1 = 0). This matters for ND-500 programs under SINTRAN, where
 * SET-TRAP-CONDITION OVERFLOW is a documented and supported thing to do.
 *
 * Bits 30 (DT) and 31 (DE) are deliberately NOT here - the manual classes them
 * non-ignorable, and NDIX enables both. Adding them to this mask would make
 * them misfire. They have no generator in the emulator yet (SOLO/TUTTI is a
 * stub), so nothing is currently being dropped on their account. */
#define TRAP_IGNORABLE_MASK 0x3FFFFA00ULL

/* Every trap the manual classes NON-ignorable: the process-switch pair DT(30)
 * and DE(31), plus the fault/interrupt range 32-41.
 *
 * Manual ND-05.009.4 Table 10 gives both DT and DE class "N A" - Non-ignorable,
 * taken After the instruction - and the Linker manual's SET-TRAP-CONDITION list
 * starts its non-ignorable section at "30D DISABLE-PROCESS-SWITCH-TIMEOUT".
 * Non-ignorable means the enable masks do not gate DISPATCH the way they do for
 * the ignorable range, so these have to take the same path as a page fault. */
#define TRAP_NONIGNORABLE_MASK (TRAP_INTERRUPT_MASK | TRAP_DT | TRAP_DE)

/* Traps handled AFTER the instruction completes (manual ND-05.009.4 Table 10,
 * "Status bits survey", column B/D/A): O(9), IVO..CT(11-19), ATF..AZ(21-24),
 * DT(30), DE(31), PWF(39). For these the saved P register (trap frame arg2,
 * what RETT returns to) points to the NEXT instruction; Trapping P (arg1)
 * still names the trapping instruction. All other traps are Before/During
 * class: arg2 == arg1 and RETT retries the trapping instruction. */
#define TRAP_AFTER_MASK 0x80C1EFFA00ULL

/* Trap system state */
typedef struct {
    int trap_occurred;           /* Flag indicating if a trap occurred */
    uint64_t trap_condition;     /* The trap condition that occurred */
    uint32_t trap_pc;            /* PC where trap occurred */
    uint32_t trap_data_addr;     /* Related data address */
    char trap_name[64];          /* Trap type name for test validation */
    char trap_description[256];  /* Human-readable trap description */
} Nd500TrapState;

extern Nd500TrapState g_trap_state;

typedef struct Nd500Regs {
    uint32_t PC;
    uint32_t FLAGS;
    uint32_t I[4];
    uint32_t A[4];
    uint32_t E[4];
    uint32_t L, B, R;
    uint32_t TOS, LL, HL, THA;
    uint32_t OTE1, OTE2, CTE1, CTE2, MTE1, MTE2, TEMM1, TEMM2;
    uint32_t ST1, ST2;  /* Status registers (64-bit) */
    uint32_t PSTP, DITBASE, CED, CAD, PS;  /* MMU registers */
} Nd500Regs;

void nd500_cpu_init(Nd500Cpu* cpu, Nd500Machine* machine);
void nd500_cpu_free(Nd500Cpu* cpu);
void nd500_cpu_reset(Nd500Cpu* cpu);
bool nd500_cpu_step(Nd500Cpu* cpu);  /* Returns false if trap occurred */
void nd500_cpu_get_regs(Nd500Cpu* cpu, Nd500Regs* out);
int nd500_cpu_run(Nd500Cpu* cpu, int steps);

/* Trap system functions */
/* Queue a front-end completion interrupt for delivery at the next instruction
 * boundary. Replaces assigning fe_int_pending/gen/sub/rpk by hand, which lost a
 * completion whenever two were outstanding at once. */
void nd500_fe_int_post(Nd500Cpu* cpu, uint32_t gen, uint32_t sub, uint32_t rpk);

/* Non-zero if any front-end completion is waiting to be delivered. */
int  nd500_fe_int_pending(Nd500Cpu* cpu);

void raise_trap(Nd500Cpu* cpu, uint64_t trapBit, uint32_t trapPC, uint32_t dataAddr);

/* Non-zero if <trapBit> is enabled for the current domain, by the own-domain
 * mask (OTE) or the mother-domain mask (MTE, read from the kernel DIT when one
 * is present). Needed by the BP instruction, which raises BPT when that trap is
 * enabled and IIC when it is not. */
int nd500_trap_is_enabled(Nd500Cpu* cpu, uint64_t trapBit);

/* CALL/ENT* sequence-interlock save stack: pushed by the trap dispatch, popped by
 * RETT, keyed on the trap frame address (THA+256). Not on the resume PC: the
 * kernel rewrites that (machine/trap.c:430,472 set cx_p = &fuerror on a failed
 * pagein), so a PC key misses and the interlock is lost. See trap_seq above. */
void nd500_trap_seq_push(Nd500Cpu* cpu, uint32_t frame_base);
void nd500_trap_seq_pop(Nd500Cpu* cpu, uint32_t frame_base);
/* LIFO pop for the lregbl trap-return NDIX uses instead of RETT. */
void nd500_trap_seq_pop_top(Nd500Cpu* cpu);
/* Apply a domain's PiA (privilege) to live ST1; privilege follows CED across
 * domain transitions (trap dispatch, RETT, domain return). No-op without a DIT. */
void nd500_apply_domain_pia(Nd500Cpu* cpu, uint32_t domain);
/* Set non-zero to suppress informational CPU-side printf output (shell clean mode). */
extern int nd500_quiet;

/*
 * 1 when nd500x is running as a LIBRARY inside another machine, 0 when it is the
 * free-running nd500x binary. Default 0.
 *
 * WHY IT EXISTS. nd500x is both, and the two have different owners of stdout. As
 * the binary, stdout is nd500x's own console and a diagnostic line there is
 * wanted. As a library - inside nd100x, where an ND-5000 station runs out of the
 * shared MFbus pool - stdout belongs to the HOST machine's guest terminal, and a
 * diagnostic written there lands in the middle of the guest's own output.
 * Measured 30-SEP-2026: "ND-500: Data MMU enabled (DMON)" appeared inside the
 * ND-500 monitor's "> Loading Swapper" line on the SINTRAN console.
 *
 * So this is not a verbosity level and must not be conflated with nd500_quiet,
 * which an operator sets. It says WHO OWNS THE STREAM. The embedder sets it once,
 * at attach time, before anything can print.
 */
extern int nd500_embedded;
void check_pending_traps(Nd500Cpu* cpu, uint32_t trappingPC);

/* Raise DT (bit 30) if an unprivileged SOLO region has run past 256
 * macroinstruction cycles. Called once per instruction; a no-op unless PSD is
 * set. See the implementation in cpu.c for the manual references. */
void check_solo_timeout(Nd500Cpu* cpu, uint32_t trappingPC);
void invoke_trap_handler(Nd500Cpu* cpu, uint64_t trapBit, uint32_t trappingP);

/* Trap state management */
void nd500_trap_clear(void);
int nd500_trap_occurred(void);
const Nd500TrapState* nd500_trap_get_state(void);

/**
 * @brief Print the most recent instruction PCs to stderr.
 *
 * Public wrapper over cpu.c's stop-diagnostics ring, for callers outside
 * cpu.c (Ret.c's bogus-domain-return diagnostic). Prints nothing unless the
 * stopdbg setting (ND500X_STOPDBG) is on.
 *
 * @param tag  Label printed at the head of the dump, naming the caller.
 */
void nd500_dump_pc_ring(const char *tag);
void nd500_trap_set_state(uint64_t condition, uint32_t pc, uint32_t data_addr, const char* description);

/* Trap helper functions */
void trap_illegal_instruction(Nd500Cpu* cpu, uint32_t pc, uint32_t opcode);
void trap_illegal_operand(Nd500Cpu* cpu, uint32_t pc);
void trap_instruction_sequence_error(Nd500Cpu* cpu, uint32_t pc);
void trap_protect_violation(Nd500Cpu* cpu, uint32_t pc, uint32_t address);
void trap_page_fault(Nd500Cpu* cpu, uint32_t pc, uint32_t address);
void trap_divide_by_zero(Nd500Cpu* cpu, uint32_t pc);
void trap_descriptor_range(Nd500Cpu* cpu, uint32_t pc);
void trap_bcd_overflow(Nd500Cpu* cpu, uint32_t pc);
void trap_floating_overflow(Nd500Cpu* cpu, uint32_t pc);
void trap_floating_underflow(Nd500Cpu* cpu, uint32_t pc);
void trap_invalid_operation(Nd500Cpu* cpu, uint32_t pc);
/* Integer overflow (bit 9). NOT the same as trap_invalid_operation - see the
 * TRAP_O comment above for why using that one was a real bug. */
void trap_integer_overflow(Nd500Cpu* cpu, uint32_t pc);
void trap_stack_overflow(Nd500Cpu* cpu, uint32_t pc);
void trap_stack_underflow(Nd500Cpu* cpu, uint32_t pc);
void trap_breakpoint(Nd500Cpu* cpu, uint32_t pc);
void trap_single_instruction(Nd500Cpu* cpu, uint32_t pc);
void trap_branch(Nd500Cpu* cpu, uint32_t pc);
void trap_call(Nd500Cpu* cpu, uint32_t pc);

/* Instruction metadata (loaded from build/src/cpu/instructions.json if available) */
int nd500_instr_load_default(void);
const char* nd500_instr_mnemonic(uint16_t opcode);
int nd500_instr_opcode_length(uint16_t opcode);
int nd500_instr_operand_count(uint16_t opcode);
int nd500_instr_has_rn(uint16_t opcode);
char nd500_instr_default_dtype(uint16_t opcode);
int nd500_instr_dest_reg(uint16_t opcode);
uint8_t nd500_instr_prefixes_mask(uint16_t opcode);
uint8_t nd500_instr_variant(uint16_t opcode);
const char* nd500_instr_dtype_prefix(uint16_t opcode);
int nd500_instr_is_branch(uint16_t opcode);
int nd500_instr_operand_is_direct(uint16_t opcode, uint8_t operand_idx);

/* Decoded instruction model */
/* Note: Nd500AddrMode and Nd500OperandDecoded defined at top of file (needed by Nd500Cpu) */

/**
 * Data type enumeration for instruction operands
 *
 * Register bank selection for REGISTER addressing mode:
 * - BYTE, HALFWORD, WORD -> Integer registers I1-I4
 * - FLOAT -> Float registers A1-A4
 * - DOUBLEWORD -> Double registers D1-D4 (A+E pairs)
 */
typedef enum {
    ND500_DTYPE_BYTE = 0,       // 8-bit (BY) -> I registers
    ND500_DTYPE_HALFWORD = 1,   // 16-bit (H) -> I registers
    ND500_DTYPE_WORD = 2,       // 32-bit integer (W) -> I registers
    ND500_DTYPE_DOUBLEWORD = 3, // 64-bit (D) -> D registers (A+E pairs)
    ND500_DTYPE_FLOAT = 4,      // 32-bit float (F) -> A registers
    ND500_DTYPE_BIT = 5         // 1-bit (BI) -> single bit in memory, I registers for value
} Nd500DataType;

#define ND500_MAX_INSTRUCTION_BYTES 2048  /* Max bytes for CALL with 255 operands */

typedef struct Nd500FetchedInstruction {
    uint32_t address;
    uint16_t opcode;
    uint8_t opcode_len;
    const char* mnemonic;
    uint8_t operand_count;
    Nd500OperandDecoded operands[ND500_MAX_OPERANDS];
    uint32_t total_len;
    uint8_t bytes[128];  /* All bytes consumed by this instruction (increased for CALL) */

    /* Pre-decoded metadata (like C# FetchedInstruction) */
    uint8_t target_register;      /* 0=none, 1-4 for register variants (I1-I4, A1-A4, etc.) */
    Nd500DataType data_type;       /* Data type: BYTE, HALFWORD, WORD, DOUBLEWORD */
    bool uses_float_registers;     /* true for Fn/Dn float variants */
} Nd500FetchedInstruction;

int nd500_decode_at(Nd500Machine* m, uint32_t pc, Nd500FetchedInstruction* out);

/* Execute one decoded instruction */
void nd500_execute_decoded(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);

/* Operand access helpers for instruction implementations */
uint32_t read_operand_w(Nd500Cpu* cpu, const Nd500OperandDecoded* op);
void write_operand_w(Nd500Cpu* cpu, const Nd500OperandDecoded* op, uint32_t value);

/* ND-100 I/O Processor Bridge Helper Functions */
uint16_t nd500_read_nd100_word(Nd500Cpu* cpu, uint32_t nd100_addr);
void nd500_write_nd100_word(Nd500Cpu* cpu, uint32_t nd100_addr, uint16_t data);



#endif /* CPU_PROTOS_H */
