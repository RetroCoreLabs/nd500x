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

    /* A HOST MACHINE'S CLAIM ON MONITOR CALLS. Installed by an embedding machine
     * whose own operating system owns them - an nd100x running SINTRAN with an
     * ND-5000 on the octobus. Returns nonzero when it has taken the call, in which
     * case no local emulation runs. See the call site in nd500_indirect.c for why
     * this outranks a PRESENT ndmonlib rather than filling in for a missing one. */
    int (*mon_call_host)(void *ctx, uint32_t mon_number, uint32_t arg_count,
                         const uint32_t *arg_addresses, uint32_t *out_resolved);
    void *mon_call_host_ctx;
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

    /* THE REST OF THE MMU LATCH, PRESERVED THE SAME WAY trap_saved_info IS.
     *
     * The walk latches three things about a fault: WHERE it failed
     * (mmu_pgf_where), WHICH physical segment (mmu_pgf_psn) and whether the
     * access was a WRITE (mmu_pgf_is_write). raise_trap copied only the first
     * into a saved field and then zeroed the live one; the other two were never
     * cleared anywhere in either repository.
     *
     * So they went stale. A trap with no MMU access in it - a stack overflow, an
     * illegal instruction - was reported to SINTRAN carrying the physical segment
     * number and the read/write direction of some EARLIER page fault. MEASURED
     * 04-OCT-2026: a stack overflow reported that way drew "Illegal physical
     * segment" from SINTRAN, which is error 22B, "illegal physical segment number
     * in a page fault" (ND-05.017.01 Appendix A) - a complaint about a field
     * nobody had set for that trap.
     *
     * Saved rather than simply cleared, because the embedding reads them AFTER
     * nd500_cpu_step returns and the live fields cannot survive that long. Zero
     * here means "not collected", which is what the reference sends in the same
     * situation (Nd500CpuProcessBridge.OnUnhandledTrap keeps mms and psn at 0
     * unless the latch is set) and is a true statement where a stale number is
     * not. */
    uint32_t trap_saved_psn;
    int      trap_saved_is_write;
    /* MMU fault-location code (MMWHERE nibble, plus MMINST 0x40 for an I-channel
     * access) for the CURRENT fault, set by the MMU walk just before it calls
     * trap_page_fault() or trap_protect_violation(). Values are the MMW_* defines
     * in nd500_mmu.h. raise_trap copies it into trap_saved_info -> cx_info, which
     * the NDIX kernel branches on (machine/trap.c): the PGF handler services only
     * PFZ2, and both protect-violation handlers attempt pagein() only for
     * PVWVIOL with MMINST clear. For a page fault, 0 => default PFZ2. */
    uint32_t mmu_pgf_where;

    /* The physical segment number the CURRENT fault resolved against, set beside
     * mmu_pgf_where by the same walk. The B30 trap-stop record carries it at 0o21 of
     * a 46B page fault, and SINTRAN's swapper needs it to know WHICH segment to page
     * into - a record with a plausible fault address and a zero segment sends it
     * after the wrong one. */
    uint32_t mmu_pgf_psn;

    /* Whether the CURRENT fault was raised by a WRITE. The B30 trap-stop record
     * carries the access class in bits 31-29 of the MMS status word - 100 read,
     * 101 write - and the swapper reads it to decide whether the page it brings in
     * must be writable. Set beside mmu_pgf_where by the same walk. */
    int mmu_pgf_is_write;

    /* Per-generic-device interrupt priority, captured from the FE_IDEV command
     * packet (machine/if.h: every _idev_cpk variant begins with "short ipl") and
     * used when that device's completion interrupt is delivered. Indexed by
     * generic device number; 0 = never connected, use the default. Without it
     * every completion went out at IPL_DK - see fe_deliver's caller. */
    uint8_t fe_dev_ipl[16];

    /* Variable operand buffer for CALL/CALLG/POLY (all operands including fixed) */
    Nd500OperandDecoded extra_operands[ND500_MAX_OPERANDS];
    uint16_t extra_operand_count;

    /* ND-100 I/O Processor Bridge Configuration.
     *
     * THE ND-100-SIDE OPERAND OF RIOM IS TRANSPORT-SPECIFIC. It is produced by
     * SINTRAN's CNVWADR, and what CNVWADR emits depends on the transport:
     *   - ND-500 3022  : a 24-bit ND-100 physical WORD address. 2 bytes per unit.
     *   - ND-5000 / octobus : a BYTE offset inside the 5MPM window. 1 byte per unit.
     *     Same convention the B30 copy microcode uses for RESIRD/RESIWR and the same
     *     one X5BEX and the mailbox LINK words use - see the flat byte-addressed
     *     window described at src/ndbus/ndbus_servicer.c:238.
     *
     * One formula covers both:
     *     host_byte = nd100_memory_offset
     *               + operand * nd100_bytes_per_unit
     *               - nd100_window_base
     *
     * The DEFAULTS BELOW ARE THE 3022 CONVENTION, which is why
     * nd100_mapping_configured exists: an embedding that never wires the mapping does
     * not fail, it silently behaves like a 3022. On an octobus that reproduces the
     * swapper fault exactly (measured 30-SEP-2026: source 0x00008E30 read as a word
     * address returned zeros, the swapper scanned a record that had never been filled
     * and reported SWPFATAL 0o201 "Fatal error from Swapper"). Reading the three values
     * back cannot tell "nobody configured this" from "configured, and these are the
     * values", and those two readings need opposite fixes - so the flag is reported as
     * its own answer and never folded into the values. */
    uint32_t nd100_memory_offset;  /* Host byte address that ND-100 operand 0 denotes */
    uint32_t nd100_bytes_per_unit; /* Host bytes per ND-100 operand unit: 1 or 2 */
    int32_t  nd100_window_base;    /* Subtracted AFTER scaling; see the formula above */
    int      nd100_mapping_configured; /* Nonzero once the mapping has been wired */

    /* THE TRAP-STOP SEAM. Ported from RetroCore's ITrapSink
     * (Emulated.HW/ND/CPU/ND500/Servicer/ITrapSink.cs), whose contract this
     * reproduces field for field.
     *
     * A trap that no LOCAL handler consumed - no THA/DIT vector installed, or no
     * handler table at all - is not necessarily a dead CPU. On a machine with an
     * ND-100 beside it, that trap is reported OUTWARD: the microcode's TRAP_GENx
     * stop writes STOPR, TRAPN and the trap record into the process's message and
     * answers it, and SINTRAN decides what to do (trap 46B, a page fault, routes
     * to the swapper). Only a machine with nobody to report to should halt.
     *
     * WHY A SINK AND NOT A STOP REASON. This emulator used to let the embedding
     * read machine->stop_reason after nd500_cpu_step() returned false. That seam
     * loses the two things the report needs:
     *   - THE TRAP NUMBER. trap_to_stop_reason() collapses 64 trap bits onto a
     *     handful of StopReason values in priority order, and DT and DE have no
     *     value at all. The number cannot be recovered afterwards.
     *   - THE MMU LATCH. raise_trap() moves mmu_pgf_where into trap_saved_info and
     *     zeroes it, so an embedding reading it later always sees 0 and composes a
     *     status word with an access class and no fault location - the value
     *     measured being rejected by SINTRAN as "NOT KNOWN TRAP".
     * Called AT THE RAISE, both are still live. That is the whole reason the
     * interface exists in the reference and the reason it is ported here.
     *
     * trap_number crosses the seam as the RAW ND TRAPN number rather than this
     * emulator's bit mask, deliberately and for the same reason the reference
     * gives: the seam then does not depend on either side's type layout. It is the
     * BIT INDEX of the condition - page fault is bit 38 = 46B, protect violation
     * bit 36 = 44B, stack overflow bit 27 = 33B.
     *
     * Returns nonzero when the trap was consumed as a stop reported outward, in
     * which case the CPU parks and the local halt is skipped. Zero means the
     * embedding did not take it and the normal local behaviour stands. A CPU with
     * no sink installed - the free-running nd500x - behaves exactly as before. */
    int (*trap_sink)(void *ctx, uint16_t trap_number, uint32_t trapping_pc,
                     uint32_t trap_address);
    void *trap_sink_ctx;

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
/* Is this domain inside a trap handler? Reads the DIT when one is configured,
 * because a parked process restarted on the same X5CPU does no context load and
 * a cached copy goes stale; falls back to the CPU field otherwise. The setter
 * writes both. */
bool nd500_is_in_trap_handler(Nd500Cpu* cpu);
void nd500_set_in_trap_handler(Nd500Cpu* cpu, bool inside);
/* The domain's inside-trap-handler flag, out of the real DIT (offset 187). Kept
 * there because no context save or load carries the C field, so a handler that
 * parks mid-fault would otherwise come back with it clear and have its RETT
 * refused. False when no DIT has been configured. */
bool nd500_dit_read_ith(Nd500Cpu* cpu, uint32_t domain);
void nd500_dit_write_ith(Nd500Cpu* cpu, uint32_t domain, bool inside);
/* The domain's trap handler address, out of the real DIT (stride 256, offset
 * 182). THA is a DIT-sourced domain register: it is present in the context
 * block but NEWCNTXT does not load it from there, so a context load must take
 * it from here. Returns 0 when no DIT has been configured. */
uint32_t nd500_dit_read_tha(Nd500Cpu* cpu, uint32_t domain);
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

/**
 * @brief Install the trap-stop seam, or remove it with a NULL sink.
 *
 * See the trap_sink field comment on Nd500Cpu for the contract. Ported from
 * RetroCore ITrapSink; the embedding (an nd100x with an ND-5000 on the octobus)
 * implements it the way Nd500CpuProcessBridge.OnUnhandledTrap does.
 *
 * @param cpu  CPU to install the sink on.
 * @param sink Callback invoked at the raise for a trap no local handler took;
 *             NULL removes any installed sink.
 * @param ctx  Opaque pointer handed back to the callback unchanged.
 * @return 0 on success, -1 when cpu is NULL.
 */
int nd500_cpu_set_trap_sink(Nd500Cpu* cpu,
                            int (*sink)(void *ctx, uint16_t trap_number,
                                        uint32_t trapping_pc, uint32_t trap_address),
                            void *ctx);

/**
 * @brief The ND TRAPN number for a trap condition: the BIT INDEX of the mask.
 *
 * Page fault (bit 38) is 46B, protect violation (bit 36) is 44B, stack overflow
 * (bit 27) is 33B. Mirrors RetroCore CpuND500.Trap.cs GetTrapNumber, which is the
 * same bit-index scan.
 *
 * @param trap_bit One trap mask (TRAP_PGF, TRAP_STO, ...). A combined mask yields
 *                 the lowest-numbered condition in it.
 * @return The trap number 0..63, or 0xFFFF when trap_bit is zero.
 */
uint16_t nd500_trap_number(uint64_t trap_bit);

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

/**
 * @brief Wire BOTH halves of the ND-100 operand mapping: the base byte address an
 *        operand of 0 denotes, and how many host bytes one operand unit spans.
 *
 * Callers must DERIVE these from the transport rather than hardcode them; the octobus
 * embedding asks the servicer for its own convention
 * (ndbus_servicer_nd100_bytes_per_unit).
 *
 * @param cpu            CPU to configure.
 * @param base_byte      Host byte address that ND-100-side operand 0 denotes.
 * @param bytes_per_unit 2 for the 3022 word-address convention, 1 for the octobus
 *                       5MPM window byte-offset convention. Any other value means the
 *                       caller derived it from something that is not an ND address
 *                       convention, and is rejected.
 * @param window_base    Bytes to subtract after scaling, turning an absolute ND-100
 *                       byte address into an ND-500 physical one. 0 on the octobus.
 * @return 0 on success, -1 if cpu is NULL or bytes_per_unit is neither 1 nor 2.
 */
/**
 * @brief Map an ND-100-side operand to a host byte address.
 *
 * One formula for both transports:
 *   base + operand * bytes_per_unit - window_base. See the field comment on
 * nd100_memory_offset in Nd500Cpu for the two conventions and why they differ.
 *
 * @param cpu        CPU holding the mapping.
 * @param nd100_addr The ND-100-side operand exactly as the program gave it.
 * @return Host byte address, or 0 when cpu is NULL or the result would be negative.
 */
uint32_t nd500_cpu_map_nd100_to_physical(const Nd500Cpu* cpu, uint32_t nd100_addr);

int nd500_cpu_set_nd100_mapping(Nd500Cpu* cpu, uint32_t base_byte,
                                uint32_t bytes_per_unit, int32_t window_base);

/**
 * @brief How far an ND-100-side operand advances per HALFWORD transferred.
 *
 * A halfword is 2 bytes, so this is 1 under the word-address convention and 2 under
 * the window byte-offset convention. RIOM steps its source with this.
 *
 * @param cpu CPU to query.
 * @return The step, or 1 when cpu is NULL.
 */
uint32_t nd500_cpu_nd100_step_per_halfword(const Nd500Cpu* cpu);
void nd500_write_nd100_word(Nd500Cpu* cpu, uint32_t nd100_addr, uint16_t data);



#endif /* CPU_PROTOS_H */
