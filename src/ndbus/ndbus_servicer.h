/**
 * @file ndbus_servicer.h
 * @brief The ND-5000 mailbox servicer: the microcode's side of the 5MPM mailbox.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * WHAT THIS IS. After ENKICK the ND-500/5000 monitor stops talking on the
 * octobus and waits on the mailbox in MPM-5 shared memory instead. Something has
 * to play the ND-5000 microprogram: walk the X5BEX ex-queue, execute the MICFU in
 * each message, write the answer status, insert the answered message into the
 * X5FIF answer ring and tell the ND-100. With nothing doing that, SINTRAN polls
 * forever and the monitor reports "ND-500(0) timeout".
 *
 * PORTED, NOT INVENTED. From
 * $RETROCORE/Emulated.HW/ND/CPU/ND500/Servicer/Nd500MicrocodeServicer.cs:
 *   ProcessChain      -> ndbus_servicer_process_chain()   (line 1751)
 *   ProcessMessage    -> ndbus_servicer_process_message() (line 2578)
 *   AnswerRingInsert  -> answer_ring_insert()             (static, below)
 * and its host seam $RETROCORE/Emulated.HW/ND/CPU/ND500/Servicer/IServicerHost.cs,
 * whose octobus implementation is OctobusND5000Station.cs lines 2146-2181.
 * The message field offsets and the MICFU codes are the ones already declared in
 * ndbus_msgqueue.h; nothing is restated here.
 *
 * ONLY ONE TRANSPORT, DELIBERATELY. RetroCore's servicer carries a generation
 * seam because it serves both the ND-500 3022 (PCB card, ND-100 WORD addresses,
 * so link << 1) and the ND-5000 octobus (MPM-5 window, WINDOW-RELATIVE BYTE
 * offsets, no shift). nd500x has no 3022 and is not getting one, so this port
 * keeps the octobus arm only: every address here is a POOL BYTE OFFSET and a
 * link value converts by identity. That is RetroCore's
 * `IServicerHost.ResolveMailboxLink` override at OctobusND5000Station.cs:2150
 * (`_mpm.Start + (linkValue & 0xFFFFFF)`) with the pool base already factored
 * out, and its inverse at :2154. Live-verified there on 21-JUL-2026: X5BEX =
 * 0xBE30 resolves to a real queue node, while the 3022's << 1 lands outside the
 * window entirely.
 *
 * WHAT IT ANSWERS SO FAR. 3RMICV (MICFU 1), the watchdog message SINTRAN sends
 * first, which reports the microprogram version. Every other MICFU answers
 * 5ERANSWER(4) - which is what the real ND-5000 does for the codes its B30 image
 * marks MSG_ILLEG, and is an honest "not implemented here" for the rest. The
 * counters below say which codes arrived, so the next one to port is measured
 * rather than guessed.
 */

#ifndef NDBUS_SERVICER_H
#define NDBUS_SERVICER_H

#include <stdbool.h>
#include <stdint.h>

#include "ndbus_msgqueue.h"
#include "ndbus_pool.h"

/** @brief MICFU codes this servicer names. Values from RetroCore N5MailboxProtocol.cs
 *  (N5MicroFunction), which cites the L07/M06 symbol tables. Octal in the ND
 *  documents, decimal here. */
#define NDBUS_MICFU_RMICV      1u   /**< 3RMICV read microprogram version */
#define NDBUS_MICFU_SWMESS     5u   /**< 05  3SWMESS message to swapper */
#define NDBUS_MICFU_DMEMRD     8u   /**< 10B DMEMRD data-memory read */
#define NDBUS_MICFU_DMEMWR     9u   /**< 11B DMEMWR data-memory write */
#define NDBUS_MICFU_CACHE     10u   /**< 12B MSG_CACHE conditional cache clears */
#define NDBUS_MICFU_RESIRD    11u   /**< 13B MSG_RESIRD resident read */
#define NDBUS_MICFU_RESIWR    12u   /**< 14B MSG_RESIWR resident write */
#define NDBUS_MICFU_MONCO     20u   /**< 24B 3MONCO monitor-call continue; shares MSG_START */
#define NDBUS_MICFU_TRACO     21u   /**< 25B 3TRACO trap continue; shares MSG_START */
#define NDBUS_MICFU_WMONCO    22u   /**< 26B 3WMONCO wait-monitor-call continue; MSG_CONWR */

/* ---- what the servicer does with a micro-function -------------------------
 *
 * THE WHOLE 16-BIT HALFWORD IS THE CODE. BIT 15 IS NOT STRIPPED.
 *
 * The reference switches on the raw halfword -
 * $RETROCORE/Emulated.HW/ND/CPU/ND500/Servicer/Nd500MicrocodeServicer.cs:2621
 * reads MICFU and :2674 is `switch ((N5MicroFunction)micfu)` with no mask - so a
 * halfword with bit 15 set matches no case and lands in `default` (:3982), which
 * answers 5ERANSWER. These accessors used to strip bit 15 first while
 * execute_micfu() switched on the raw value, so one message was classed as a
 * continue and then dispatched as unknown. They now agree with each other and
 * with the reference: a code of 64 or more, flagged or not, is out of range.
 *
 * NOT PORTED: the reference THROWS in `default` while StrictUnknownMessages is
 * true (:3997). C has no exception to throw, so this port takes the non-strict
 * path (:4008-4010): a log line and 5ERANSWER.
 *
 * ND500-MAILBOX-MESSAGE-CATALOG.md:209 reads the microcode as stripping bit 15
 * before its 64-entry dispatch, graded [V/D] - part verified, part derived. That
 * reading is NOT implemented here, by order of 05-OCT-2026: the C# is the oracle.
 */
#define NDBUS_MICFU_CLASS_NONE     0u  /**< not dispatched by this emulator */
#define NDBUS_MICFU_CLASS_INLINE   1u  /**< answered where it is, process untouched */
#define NDBUS_MICFU_CLASS_START    2u  /**< loads a context block and starts a process */
#define NDBUS_MICFU_CLASS_CONTINUE 3u  /**< resumes a parked process in place */

/**
 * @brief The table index for this MICFU halfword.
 * @param micfu_halfword The raw halfword from the message. Nothing is masked off.
 * @return 0..63 when the halfword is one of those values, otherwise 64 - a value
 *         no table entry can have, so it cannot be mistaken for one. A halfword
 *         with bit 15 set is therefore always 64.
 */
uint16_t ndbus_micfu_dispatch_code(uint16_t micfu_halfword);

/**
 * @brief What class of thing this micro-function is: NDBUS_MICFU_CLASS_*.
 * @param micfu_halfword The raw halfword; nothing is masked off.
 * @return The class, or NDBUS_MICFU_CLASS_NONE for out of range or unimplemented.
 */
uint8_t ndbus_micfu_class(uint16_t micfu_halfword);

/**
 * @brief The ND mnemonic for a micro-function, for a log line that reads.
 * @param micfu_halfword The raw halfword.
 * @return A static string; "out-of-range" or "unknown" rather than NULL.
 */
const char *ndbus_micfu_name(uint16_t micfu_halfword);
#define NDBUS_MICFU_PHYSRD    24u   /**< 30B PHYSRD physical-memory read */
#define NDBUS_MICFU_PHYSWR    25u   /**< 31B PHYSWR physical-memory write */
#define NDBUS_MICFU_IMEMRD    28u   /**< 34B IMEMRD instruction-memory read. On the
                                  *   ND-500 the SAME code is the answer-only symbol
                                  *   3MONO - a namespace collision, settled per
                                  *   generation: the B30 microcode dispatches 34B to
                                  *   the direction-fixed addrB->addrA block copy. */
#define NDBUS_MICFU_IMEMWR    29u   /**< 35B IMEMWR instruction-memory write */
#define NDBUS_MICFU_WREG      17u   /**< 21B 3WREG register write; MSG_ILLEG on the B30 */
#define NDBUS_MICFU_RPREG     36u   /**< 44B 3RPREG read P register; see the table */
#define NDBUS_MICFU_STARTP0   18u   /**< 22B MSG_STARTP0 start process 0 */
#define NDBUS_MICFU_START     19u   /**< 23B 3START start process */

/**
 * @brief Is this MICFU a CONTINUE - a message that resumes the process already
 *        loaded on the CPU, rather than loading a context block?
 *
 * 3MONCO and 3TRACO only. The microcode's context switch at 011473B compares the
 * loaded process against the wanted one and, when they match, skips BOTH the save
 * and the load, so a continue carries on from the live registers.
 *
 * 3START and MSG_STARTP0 are deliberately NOT continues even for the process that
 * is already loaded. A monitor-call stop leaves the ND-500 through
 * CALL_MON -> SET_IDLE, which marks "no current process", and the B30 IDLE loop at
 * 0o24724-25 then skips CNTXTSAVE so that NEWCNTXT/CNTXTLOAD reads the block
 * SINTRAN has just filled - the process ENTRY POINT, not the parked return address.
 * MEASURED on the octobus macro lane: after an explicit START-SWAPPER the swapper
 * makes a fresh first call, FUNCV=0 and SWPINFO=0, because its initialisation ran.
 *
 * @param micfu The raw micro-function code from the message.
 * @return true for 3MONCO and 3TRACO, false for every other code.
 */
bool ndbus_micfu_is_continue(uint16_t micfu);

/**
 * @brief Is this MICFU one of the messages that are offered to the process host?
 *
 * 3START (23B), 3MONCO (24B), 3TRACO (25B) and 3WMONCO (26B).
 *
 * 22B MSG_STARTP0 IS NOT ONE OF THEM. The reference answers it ANSWER at once and
 * never calls its process host -
 * $RETROCORE/Emulated.HW/ND/CPU/ND500/Servicer/Nd500MicrocodeServicer.cs:3314-3330
 * - so here it is NDBUS_MICFU_CLASS_INLINE and this returns false for it.
 *
 * @param micfu The raw micro-function code from the message.
 * @return true for 23B, 24B, 25B and 26B, false otherwise.
 */
bool ndbus_micfu_is_start_class(uint16_t micfu);

/** @brief N5STA message status values. RetroCore N5MessageStatus; 0 = free is
 *  INFERRED there (no symbol), the other four are symbol-verified. */
#define NDBUS_N5STA_FREE          0u
#define NDBUS_N5STA_TO_ND500      1u  /**< MSGN500: the only state the walk serves */
#define NDBUS_N5STA_WAITING       2u  /**< WAITING: in process */
#define NDBUS_N5STA_ANSWER        3u  /**< ANSWER: answer to the ND-100 */
#define NDBUS_N5STA_ERROR_ANSWER  4u  /**< 5ERANSWER: error return */

/** @brief Mask selecting the status itself. Bits 15-13 (160000B) are power-fail
 *  flags that belong to the ND-100 driver and are PRESERVED across an answer. */
#define NDBUS_N5STA_MASK       0x1FFFu
/** @brief The power-fail flag bits the answer preserves. */
#define NDBUS_N5STA_PF_MASK    0xE000u

/** @brief Link value ending a chain. */
#define NDBUS_SERVICER_LINK_END 0xFFFFFFFFu

/** @brief Bytes per context (register) block: 400B = 256. ND-05.017.01 App. A.1. */
#define NDBUS_SERVICER_CTX_STRIDE 256u

/** @brief Longest chain the walk follows before giving up. An emulator guard
 *  against a cycle, not microcode behaviour - the microcode only tests for -1. */
/** How many block copies are named in the log before it falls silent. A count says
 *  a transfer happened; only the address says whether it went where the guest
 *  meant. */
/** How many block copies a run logs. A measured run performs a few dozen, so
 *  this is a runaway guard rather than a filter; it was 24, which silently cut a
 *  run's copy log in half and made a destination census read as a negative. The
 *  gate announces itself when it is reached. */
#define NDBUS_SERVICER_COPY_LOG_LIMIT 4096u

/** How many ND-5000 processes the servicer remembers a message for, indexed by the
 *  zero-based X5CPU of the message. 64, the reference's MaxProcesses at
 *  $RETROCORE/Emulated.HW/ND/CPU/ND500/Servicer/Nd500MicrocodeServicer.cs:1164.
 *  It was 8 here, which silently dropped every process from X5CPU 8 upward. */
#define NDBUS_SERVICER_MAX_PROCESSES 64u

/** Bytes in the "already logged" bitmap for out-of-range X5CPU values: one bit
 *  for each of the 65536 values a 16-bit X5CPU field can hold. */
#define NDBUS_SERVICER_X5CPU_BITMAP_BYTES 8192u

/** How many DMEMRD and how many DMEMWR requests are logged with their raw
 *  ND-100 buffer field - see perform_dmemrd() in ndbus_servicer.c for the open
 *  question that log exists to settle. */
#define NDBUS_SERVICER_DMEM_LOG_LIMIT 8u

/** STOPR value that says "this process stopped on a trap" - TRAPCODE. */
#define NDBUS_STOPR_TRAPCODE 2u

/** STOPR value that says "this process stopped to make a monitor call" - MOCALL. */
#define NDBUS_STOPR_MOCALL 1u

/** The microcode's own argument slot limit: CALL_MON checks the count against
 *  sixteen, and the message carries exactly sixteen address and sixteen value
 *  slots. A larger count writes past them. */
#define NDBUS_MON_MAX_ARGS 16u

/** Argument ADDRESS slots, 0o40 + 2k as halfwords = byte 0x40 + 4k. */
#define NDBUS_MON_ARG_ADDR_BASE 0x40u

/** Argument VALUE slots, 0o100 + 2k as halfwords = byte 0x80 + 4k. */
#define NDBUS_MON_ARG_VALUE_BASE 0x80u

/* THE INLINE USER BUFFER for the OUTPUT monitor calls - the microcode's job,
 * hence the servicer's.
 *
 * MP-P2-N500.NPL:140656 tests MIFLAG bit WSMC and takes one of two arms. With the
 * bit CLEAR SINTRAN sends 3RMED (MICFU 10B) and fetches the buffer itself; with
 * the bit SET it reads the buffer straight out of the message through ABUFA.
 * MEASURED on the RetroCore lane with a whole-run MICFU tally: 10B never appears,
 * so SINTRAN NEVER asks. It takes the inline arm and prints whatever happens to be
 * at ABUFA - which is why a report comes out as structured garbage while the
 * program's own buffer holds the right text.
 *
 * The microcode's inline-copy set is exactly {504B, 511B, 512B} - CALL_5XX
 * 004013B-004016B into CALL_5_MATCH 013667B, screened at CALL_END 013613. For
 * those three the buffer is copied into the message BEFORE the process stops, so
 * the ND-100 never needs a second fetch.
 *
 * 513B IS DELIBERATELY EXCLUDED even though it shares the ND-100 handler body with
 * 512B. Sharing a SINTRAN handler is not sharing a microcode obligation; adding a
 * number because it is adjacent is the adjacency-is-not-dispatch error. */

/** 504B NOUTS. Layout MEASURED live: arg[1] value = byte count, arg[2] ADDRESS =
 *  the buffer. */
#define NDBUS_MON_504B_NOUTS 0x144u

/** 511B DVIO. MP-P2-N500.NPL:140627 is `SUBR DVIO,NOUTSTR` with BOTH LABELS ON ONE
 *  ADDRESS, so the outbound copy is identical. DVIO is BIDIRECTIONAL and reads
 *  bytes back afterwards through DVINST; only the OUTBOUND leg is done here and
 *  whether the return leg needs anything is UNVERIFIED. */
#define NDBUS_MON_511B_DVIO 0x149u

/** 512B A5XMSG. Microcode-verified: the copy routine at 010662 is entered by all
 *  three and reads the MON number nowhere, so there is no per-MON layout left. */
#define NDBUS_MON_512B_A5XMSG 0x14Au

/** ABUFA, the inline buffer pointer, halfwords 0o140-0o141 of the message. It is a
 *  WORD address - see ndbus_servicer_inline_buffer_target(). */
#define NDBUS_MON_ABUFA_WORD 96u

/** The microcode's own ceiling on an inline copy: 0o4000 bytes, read out of the
 *  guard at 010717. An OVER-SIZE count makes the microcode copy NOTHING - it does
 *  not clamp and copy a prefix. Copying a truncated buffer would be the divergence,
 *  and it is the "helpful" change someone will reach for. */
#define NDBUS_MON_INLINE_MAX_BYTES 2048u

/** MIFLAG sits BEFORE the message base: MIFLA = 0o177770 is halfword -8, so byte
 *  -16. WSMC is bit 0 of it and means "the data buffer is in the communication
 *  buffer". */
#define NDBUS_MON_MIFLAG_BACK_BYTES 16u
#define NDBUS_MON_MIFLAG_WSMC 0x0001u

/** How many MON 377B swapper requests a run logs with their words. A run makes
 *  a few dozen, so this is a runaway guard rather than a filter; it was 24,
 *  which silently cut a 36-request run's log to 20 and made a SWPFU census read
 *  off it wrong. The gate announces itself when it is reached. */
#define NDBUS_SERVICER_SWPWORDS_LOG_LIMIT 4096u

/** Trap 46B, the page fault. The only stop trap with the TRAP_GEN4 record layout. */
#define NDBUS_TRAP_PAGE_FAULT 0x26u

#define NDBUS_SERVICER_MAX_CHAIN 64

/** @brief MICFU codes the histogram counts. */
#define NDBUS_SERVICER_MICFU_COUNTS 64u

/** @brief The microprogram version 3RMICV reports when nothing overrides it.
 *  027232B = 11930, which RetroCore records as the 5800 image's own stamp
 *  (Nd500MicrocodeServicer.cs:57). The real answer lives in control-store word 0
 *  part CS0; until this servicer is given that handle the value is a default and
 *  is labelled one. */
#define NDBUS_SERVICER_MICRO_VERSION_DEFAULT 0x2E9Au

/** @brief The CPU parameter 3RMICV reports in the second halfword. 001741B =
 *  ND-5000 model 8 (5800) CPUPAR, the pre-CS-load fallback RetroCore sets at
 *  OctobusND5000Station.cs:720. SINTRAN does not consume it (carver R5); it is
 *  written because the microcode writes it. */
#define NDBUS_SERVICER_CPU_PARAMETER_DEFAULT 0x03E1u

/** @brief What the servicer needs from whoever owns the shared memory. */
typedef struct NdbusServicerHost
{
    /** Opaque owner, handed back to the callbacks. */
    void *ctx;

    /**
     * The answer status has been written and the ring insert is done: tell the
     * ND-100. RetroCore IServicerHost.AnswerWritten; on the octobus this is the
     * GIVEINT interrupt frame. May be NULL, in which case nothing is signalled
     * and the ND-100 finds the answer only by polling.
     */
    void (*answer_written)(void *ctx, uint32_t msg_byte);

    /** Diagnostic line. May be NULL. */
    void (*log)(void *ctx, const char *message);

    /**
     * The trap-config writes have named a Domain Information Table base: declare it
     * on the CPU before the process starts.
     *
     * DECLARING IS NOT SETTING UP, and the distinction is the whole reason this
     * callback is separate from start_process. The table's entries are whole
     * 256-byte process control blocks that the guest has ALREADY filled by the time
     * this fires; an implementation that zeroed them while declaring the base would
     * erase precisely those writes, and the resulting zero trap-handler address
     * reads as though declaring had done nothing.
     *
     * May be NULL, in which case the base is only recorded in this struct.
     *
     * @param ctx  The owner.
     * @param base Pool byte offset of the table. MAY LEGITIMATELY BE ZERO.
     */
    void (*declare_dit_base)(void *ctx, uint32_t base);

    /**
     * A start-class message arrived: start the process on the real ND-5000 CPU.
     *
     * Ported from RetroCore INd500ProcessHost.OnStartProcessND5000. The ND-5000
     * variant, because on this generation there is NO 21B register image - 3WREG is
     * MSG_ILLEG on the B30 - so the context comes from the per-process CONTEXT
     * BLOCK the microcode's NEWCNTXT loads, and the servicer computes that block's
     * address and passes it in.
     *
     * @param ctx           The owner.
     * @param msg_byte      The activation message. It becomes the process's
     *                      answer-in-place message, so an implementation that takes
     *                      the start MUST remember it for the stop.
     * @param micfu         The raw MICFU code.
     * @param ctx_byte      Pool byte offset of this process's context block.
     * @return true  = TAKEN. The servicer leaves the message WAITING and does NOT
     *                 answer it; the process's stop answers later.
     *         false = not taken, no CPU or not runnable. The servicer answers
     *                 immediately, which is the behaviour with no host at all.
     *
     * May be NULL, which means the same as always declining.
     */
    bool (*start_process)(void *ctx, uint32_t msg_byte, uint16_t micfu, uint32_t ctx_byte);

    /**
     * Read ND-500 DATA memory through the MMU, in the calling process's context.
     *
     * Ported from RetroCore INd500ProcessHost.TryReadDataBytes
     * (INd500ProcessHost.cs:75). The servicer sits below the CPU and cannot walk a
     * process's page tables itself, so the owner that holds the CPU does it.
     *
     * Needed for the inline user buffer of the output monitor calls: the microcode
     * copies the program's buffer into the message before the process stops, and
     * the buffer is named by an ND-500 LOGICAL address.
     *
     * May be NULL, which means decline. A host that declines leaves the buffer
     * alone, and SINTRAN then reads stale message content and prints it - so a
     * decline is logged rather than passed over.
     *
     * @param ctx             The owner.
     * @param logical_address ND-500 data-space logical address of the first byte.
     * @param destination     Buffer to fill, at least count bytes.
     * @param count           How many bytes.
     * @return true only when EVERY byte translated and was read. A partial read is
     *         a decline: half a buffer printed as text is a wrong answer that looks
     *         like an answer.
     */
    bool (*read_nd500_data_bytes)(void *ctx, uint32_t logical_address,
                                  uint8_t *destination, uint32_t count);

    /**
     * Make the process a message names the one loaded on the CPU, before a
     * DMEMRD (10B) or DMEMWR (11B) translates its logical address.
     *
     * Ported from RetroCore INd500ProcessHost.TryLoadNamedProcess
     * ($RETROCORE/Emulated.HW/ND/CPU/ND500/Servicer/INd500ProcessHost.cs:123).
     * The servicer calls it and then goes on with the transfer WHATEVER it
     * returns, exactly as Nd500MicrocodeServicer.cs:2868 and :3135 do - the host
     * is the one that reports a failure.
     *
     * May be NULL, which means the same as the reference's default
     * implementation: nothing to load, carry on.
     */
    bool (*load_named_process)(void *ctx, uint16_t x5cpu);   /* C# INd500ProcessHost.TryLoadNamedProcess: make X5CPU the loaded process; false if it cannot */

    /**
     * Write ND-500 DATA memory through the MMU, in the loaded process's context.
     * The mirror of read_nd500_data_bytes, needed by DMEMWR (11B).
     *
     * Ported from RetroCore INd500ProcessHost.TryWriteDataBytes
     * ($RETROCORE/Emulated.HW/ND/CPU/ND500/Servicer/INd500ProcessHost.cs:102).
     *
     * May be NULL, which means decline: DMEMWR then answers 5ERANSWER and
     * nothing is written anywhere.
     *
     * @return true only when every byte was written.
     */
    bool (*write_nd500_data_bytes)(void *ctx, uint32_t logical_address, const uint8_t *source, uint32_t count);   /* C# TryWriteDataBytes */

    /* THE RESTART SEAM IS NOT HERE YET, AND THAT IS DELIBERATE.
     *
     * RetroCore's INd500ProcessHost has a DEDICATED callback per restart kind -
     * OnMonitorCallRestart and OnWaitMonitorCallRestart - and its servicer
     * DECODES THE ANSWER AND PUSHES IT as arguments. Ours does not: the host gets
     * the generic start_process above and the embedding then reads the answer back
     * out of the message itself with ndbus_servicer_read_monitor_result().
     *
     * That inversion is a real divergence in the logical design and it should be
     * fixed, because three of the message's slots mean something different coming
     * back than going out (MCNO/MSWMC carry FUNCV, STOPR carries KFLIP, NUMPA is a
     * bitmask and not a count), so a host that re-reads them is a SECOND decoder
     * of the same words and a second place to get them wrong.
     *
     * The two callbacks were added here and then removed again on 04-OCT-2026
     * rather than left in place unused: the servicer offered the seam, no host
     * implemented it, and the embedding went on pulling - which is worse than
     * either design on its own. Converting the bridge's continue arm is the other
     * half and is a refactor of the live message path, so it belongs in a session
     * where it can be reviewed and run on the real lane, not bolted on.
     */
} NdbusServicerHost;

/** @brief One mailbox servicer. */
typedef struct NdbusServicer
{
    NdbusPool         *pool;   /**< the shared MPM-5 memory every address is in */
    NdbusServicerHost  host;   /**< the owner's callbacks */

    /** Pool byte offset of the X500DF global header, or 0 when no mailbox has
     *  been located yet - in which case the answer path writes N5STA and does
     *  NOT touch the ring or the semaphore. */
    uint32_t header_base;

    /** ND-100 physical BYTE address of pool byte 0: where the shared window sits
     *  in the ND-100's address space (the monitor's "ND-500 address zero").
     *
     *  Links, X5FIF and the copy-family operands are already window-relative, so
     *  nothing else needs this. ABUFA is the exception: SINTRAN stores an ND-100
     *  PHYSICAL address there (MP-P2-N500.NPL:140675, "% ND-100 PHYSICAL ADDR"),
     *  and RetroCore hands it to host.WriteNd100Word / ReadNd100Word, which take
     *  ND-100 physical addresses. MEASURED 05-OCT-2026: ABUFA read 0x00216800
     *  words = ND-100 byte 0x42D000 = window base 0x420000 + 0xD000, while the
     *  copy family named the same buffer as pool 0x00D000.
     *
     *  0 until the owner sets it with ndbus_servicer_set_nd100_window_base(),
     *  which is also the right value for a pool that IS the ND-100's view. */
    uint32_t nd100_window_base_byte;

    uint16_t micro_version;  /**< what 3RMICV reports; see the default above */
    uint16_t cpu_parameter;  /**< the second 3RMICV halfword */

    /* Diagnostics. Every one of these answers a question that is otherwise
     * indistinguishable from outside - RetroCore learned that an insert skipped
     * under a debug-only counter reads as a healthy run. */
    unsigned long messages_processed;        /**< messages that reached the MICFU switch */
    unsigned long messages_answered;         /**< answers written (ANSWER or 5ERANSWER) */
    unsigned long messages_declined;         /**< 5ERANSWER answers among those */
    unsigned long nodes_walked;              /**< chain nodes the walk visited */
    unsigned long nodes_not_ours;            /**< nodes whose N5STA was not MSGN500 */
    unsigned long answer_ring_inserted;      /**< ring inserts that happened */
    unsigned long answer_ring_skipped_init;  /**< skipped: ring not initialised */
    unsigned long answer_ring_skipped_full;  /**< skipped: ring full (still answered) */
    unsigned long answer_sem_taken;          /**< X5SEM held across the answer */
    unsigned long answer_sem_not_taken;      /**< X5SEM never freed; answered unlocked */
    unsigned long answer_no_header;          /**< answers written with no mailbox header */

    /** How many of each MICFU reached the switch, so the next code to port is
     *  measured. Codes at or above NDBUS_SERVICER_MICFU_COUNTS are not counted. */
    unsigned long micfu_counts[NDBUS_SERVICER_MICFU_COUNTS];

    /** The activation message of each started process, indexed by X5CPU and NEVER
     *  cleared - see ndbus_servicer_answer_trap_stop() for the two measured ways a
     *  single "active message" field gets this wrong. */
    uint32_t      process_msg[NDBUS_SERVICER_MAX_PROCESSES];

    /** WHICH STOP EACH PROCESS IS PARKED ON, because the restart's answer slots
     *  are a UNION and the arm depends on the message KIND.
     *
     *  The reference states the rule on the slots themselves (MSG_CONMC carve
     *  015676-751): KFLIP re-uses the STOPR slot at 0o11, the write-back MASK
     *  re-uses NUMPA at 0o12, and FUNCV spans MCNO/MSWMC at 0o13-0o14 - "SIX
     *  unions live in this block, so the arm depends on the message KIND".
     *
     *  A TRAP stop writes the trap record into those same halfwords: STOPR
     *  becomes TRAPCODE(2), the saved P occupies 0o13-0o14, TRAPN sits at 0o16.
     *  Decoding the monitor-call arm over that reads the emulator's OWN trap
     *  record back as an answer. MEASURED 2026-10-04: a 3MONCO restart of a
     *  process parked on trap 46B produced K=1 from the TRAPCODE value and
     *  FUNCV=0x467F0800 from the trapping P 0x0800467F with its halves swapped,
     *  put that in I1, and the process faulted on it at once - trap 44B, "no
     *  data capability, segment 8", which is the protection violation the
     *  console then reported.
     *
     *  NDBUS_STOPR_MOCALL or NDBUS_STOPR_TRAPCODE; 0 when the process has not
     *  stopped. */
    uint16_t      process_stop_kind[NDBUS_SERVICER_MAX_PROCESSES];

    unsigned long trap_stops_attempted; /**< counted before anything can refuse */
    unsigned long trap_stops_posted;    /**< records actually written */
    unsigned long trap_stops_declined;  /**< no message recorded for that X5CPU */
    unsigned long page_faults_posted;   /**< of those, 46B records */
    unsigned long mon_calls_attempted;
    unsigned long mon_calls_posted;
    unsigned long mon_calls_declined;
    unsigned long mon_results_read;
    uint16_t      last_mon_number;
    uint16_t      last_trap_number;
    uint32_t      last_trap_pc;
    uint32_t      last_trap_address;

    /* Block-copy diagnostics. */
    unsigned long copies_done;         /**< copy-family transfers performed */
    unsigned long copy_bytes;          /**< bytes moved by them in total */
    /** NO LONGER INCREMENTED. The copy family used to refuse a transfer that left
     *  the pool and answer 5ERANSWER; the reference never refuses one
     *  (Nd500MicrocodeServicer.cs:2707-2709 and :3022-3024 set understood = true
     *  unconditionally), so this port no longer does either. The field is kept
     *  only so code that reads it still compiles; copies_outside_pool below is
     *  the counter that now says a transfer left the pool. */
    unsigned long copies_refused;
    /** Copy-family transfers with at least one end outside the pool. They are
     *  performed and answered ANSWER as the reference does: a source word outside
     *  the pool reads 0, a destination word outside the pool is dropped. */
    uint32_t copies_outside_pool;
    /** DMEMRD/DMEMWR answered 5ERANSWER, for any of the reference's reasons: no
     *  host callback, a non-zero DIT number, a count above 2048, an ND-100 buffer
     *  address of 0, or the host's read or write failing. */
    unsigned long logical_copies_refused;
    uint32_t dmemrd_seen;    /**< 10B DMEMRD messages that reached perform_dmemrd() */
    uint32_t dmemrd_served;  /**< ... of those, answered ANSWER with bytes moved */
    uint32_t dmemwr_seen;    /**< 11B DMEMWR messages that reached perform_dmemwr() */
    uint32_t dmemwr_served;  /**< ... of those, answered ANSWER with bytes moved */

    /** Calls that named an X5CPU at or above NDBUS_SERVICER_MAX_PROCESSES and
     *  were therefore refused, or whose message could not be remembered. Every
     *  such call is counted; each distinct X5CPU is logged once. */
    uint32_t x5cpu_out_of_range;
    /** One bit per X5CPU value: set once that value's out-of-range line has been
     *  logged, so a process that is asked about on every trap logs one line. */
    uint8_t  x5cpu_out_of_range_logged[NDBUS_SERVICER_X5CPU_BITMAP_BYTES];
    unsigned long segment_resolved;    /**< PHYSRD/PHYSWR addresses resolved through the PST */
    unsigned long segment_unresolved;  /**< ... and those that fell back to a flat address */

    /**
     * Pool byte offset of the ND-500 physical segment table, or 0 when unknown.
     *
     * NOTHING SETS THIS TODAY, and that is the ported behaviour, not an omission
     * to paper over. RetroCore's octobus station does NOT implement
     * `IServicerHost.TryGetPhysicalSegmentTableBase` - the interface's own remark
     * says only the ND500 3022/5015 host, which owns the control store on that
     * generation, can answer it - so on the ND-5000 lane its segment resolution
     * also fails and falls back to a flat address with a counted log. This port
     * behaves the same way.
     *
     * A CANDIDATE SOURCE EXISTS AND IS NOT IMPLEMENTED: RetroCore records that
     * SINTRAN patches the base into control-store word 007771B's LARG field
     * before micro-start, measured on a real boot 2026-08-23 as 0o650000, which
     * is character for character what the monitor's MEMORY-CONFIGURATION prints
     * for "Physical segment table". This station does hold its control store, so
     * reading it there is possible - but it is NOT what the reference does on this
     * lane, so it is recorded here as the next thing to try rather than done
     * quietly. Set by ndbus_servicer_set_pst_base() when something legitimately
     * knows the answer.
     */
    uint32_t pst_base;

    /**
     * Pool byte offset of the start of the context (register) block area, or 0 when
     * the control store has not been patched.
     *
     * NOT INVENTED AND NOT A PLACEHOLDER. SINTRAN patches it into control-store
     * cell OFFSET (0o20) at CS-load time, and the microcode reads it from there -
     * GET_CNTXT at 0o13372 and CNTXTLOAD at 0o14746 both jump to that cell.
     * ND-05.017.01 Appendix A.1 names it outright, and unlike PSTBASE this cell is
     * ALREADY A BYTE ADDRESS, so it is not shifted. A process's block is then
     *     area + 400B + process_no * 400B
     * with 400B = 256 bytes, the leading 400B being the always-dummy first block.
     *
     * READ THE FULL 32 BITS. RetroCore records that a halfword-7-only read of the
     * sibling cell returned 0xA000 instead of 0x2A000 - a plausible-looking wrong
     * value - and feeding that on made the 23B start run with a garbage context.
     */
    uint32_t context_area_base;

    /**
     * What the trap-config writes revealed about the Domain Information Table.
     *
     * SINTRAN does not send the DIT base in a message. It WRITES the domain's
     * trap-control fields into a process control block with a run of PHYSWR
     * transfers before it starts anything - measured order on a live boot: one
     * cache-clear, thirteen PHYSWR, then the 3START - and the containing 256-byte
     * block IS the table's base. So the base is learned by watching where those
     * writes land, which is what RetroCore's servicer does.
     *
     * GUARD ON THE COUNT, NEVER ON THE VALUE. Zero is a legitimate DIT base, so
     * "nothing was learned" and "the base is zero" are the same number - the
     * reference says exactly that. dit_writes_seen is the only honest test for
     * whether dit_base means anything.
     */
    unsigned long dit_writes_seen;
    uint32_t      dit_base;

    /** Start-class messages seen, and how many a host actually took. */
    unsigned long starts_seen;
    unsigned long starts_taken;
    unsigned long starts_declined;
} NdbusServicer;

/**
 * @brief Bring a servicer up against a pool, with no mailbox located yet.
 *
 * @param sv   The servicer to initialise.
 * @param pool The shared MPM-5 memory. Every address the servicer handles is a
 *             byte offset into it.
 * @param host The owner's callbacks. Copied. Either callback may be NULL.
 * @return true on success; false having configured NOTHING when sv or pool is
 *         NULL.
 */
bool ndbus_servicer_init(NdbusServicer *sv, NdbusPool *pool, const NdbusServicerHost *host);

/**
 * @brief Tell the servicer where the shared window sits in the ND-100's memory.
 *
 * Needed only for ABUFA, the one message field that carries an ND-100 physical
 * address instead of a window-relative one. See NdbusServicer.nd100_window_base_byte.
 *
 * @param sv        The servicer. NULL is ignored.
 * @param base_byte ND-100 physical BYTE address of pool byte 0.
 */
void ndbus_servicer_set_nd100_window_base(NdbusServicer *sv, uint32_t base_byte);

/**
 * @brief Tell the servicer where the X500DF global header sits.
 *
 * Until this is called, header_base is 0 and the answer path writes only N5STA -
 * no ring insert, no semaphore. That is the ND-500 3022's behaviour in RetroCore
 * and the right thing here too: a ring insert at a guessed base corrupts.
 *
 * @param sv          The servicer.
 * @param header_byte Pool byte offset of the global header, or 0 to clear it.
 * @return true on success; false when sv is NULL.
 */
bool ndbus_servicer_set_header(NdbusServicer *sv, uint32_t header_byte);

/**
 * @brief Execute one message: the activate/answer engine.
 *
 * Reads N5STA and serves the message only when it is MSGN500(1) - RetroCore
 * ProcessMessage's first gate, and the one that makes a node parked in an
 * ND-100-side swapper state (SWPPI 6, PSWWA 7) be walked past untouched rather
 * than answered. Marks WAITING(2) while executing, executes the MICFU, then
 * writes ANSWER(3) or 5ERANSWER(4) preserving the power-fail bits, inserts into
 * the X5FIF ring under X5SEM when a header is known, and calls answer_written.
 *
 * @param sv       The servicer.
 * @param msg_byte Pool byte offset of the message block.
 * @return true when the message was executed and answered; false when it was not
 *         addressed to the ND-500, or on a bad argument. FALSE IS NOT AN ERROR -
 *         it is also the ordinary "this node is not ours" outcome.
 */
bool ndbus_servicer_process_message(NdbusServicer *sv, uint32_t msg_byte);

/**
 * @brief Walk a message chain from its head, serving every node that is ours.
 *
 * The microcode's MSG_NEXTL loop: serve, follow the 32-bit LINK at message word
 * 0 (high halfword first), stop on -1. The link is read BEFORE the message is
 * answered, because once ANSWER(3) is visible the ND-100 may relink or free the
 * block - RetroCore's octobus review flag F-oct-1, and the same reasoning applies
 * here since answer_written can interrupt the ND-100 mid-walk.
 *
 * A link of 0 stops the walk. That is an emulator guard, not microcode behaviour:
 * the microcode tests only -1, but SINTRAN zero-fills freed blocks, so a zero
 * link means the chain was torn down under the walk.
 *
 * @param sv        The servicer.
 * @param head_byte Pool byte offset of the first message block.
 * @return true when at least one message was executed and answered.
 */
bool ndbus_servicer_process_chain(NdbusServicer *sv, uint32_t head_byte);

/**
 * @brief Tell SINTRAN that a process stopped on a trap, by writing the trap record
 *        into that process's own activation message and answering it in place.
 *
 * THIS IS THE ONLY WAY SINTRAN LEARNS THE ND-5000 TRAPPED. Without it the process
 * parks and the monitor eventually reports that the swapper stopped, with STOPR and
 * TRAPN both zero, so the swapper has no fault address to page in.
 *
 * The record is the B30 one, which is NOT the ND-500 one - an earlier reading that
 * the two layouts were identical was wrong. Common header: STOPR := TRAPCODE, the
 * saved P in halfwords 0o12-0o13 and again 0o14-0o15, TRAPN := the trap number, and
 * the fault logical address at 0o17-0o20 for every stop trap. Then 46B puts the
 * physical segment at 0o21 and the MMS status at 0o22-0o23, while every other stop
 * trap puts the MMS status at 0o21-0o22 and the physical segment at 0o25.
 *
 * @param sv               The servicer.
 * @param x5cpu            Which process trapped, zero-based, as X5CPU numbers them.
 * @param trap_number      The ND trap number, e.g. NDBUS_TRAP_PAGE_FAULT.
 * @param trapping_pc      The address to RESUME at - for a retryable fault that is
 *                         the faulting instruction, not the one after it.
 * @param trap_address     The logical address that faulted.
 * @param mms_status       The composed memory-management status word.
 * @param physical_segment The physical segment the fault resolved against.
 * @return true once the record is written and the message answered; false when the
 *         servicer has no message recorded for that X5CPU, which is counted and
 *         logged rather than passed over.
 */
bool ndbus_servicer_answer_trap_stop(NdbusServicer *sv, uint16_t x5cpu, uint16_t trap_number,
                                     uint32_t trapping_pc, uint32_t trap_address,
                                     uint32_t mms_status, uint16_t physical_segment);

/**
 * @brief Send a process's MONITOR CALL back to SINTRAN, on that process's own
 *        activation message.
 *
 * AN ND-500 MONITOR CALL IS NOT SERVED ON THE ND-500. The program executes its
 * call, the process stops, and the record goes to SINTRAN on the ND-100, which
 * performs the call and restarts the process with 3MONCO (24B). A station that
 * tried to answer monitor calls locally would be emulating the wrong machine's
 * operating system.
 *
 * The record: the saved P in the halfword pair the copy family calls addrA (0o7
 * high, 0o10 low), STOPR := MOCALL, NUMPA := the argument count, MCNO := the
 * monitor number, then each argument's ADDRESS at 0o40 + 2k and its VALUE at
 * 0o100 + 2k, both 32-bit. The count is clamped to the microcode's sixteen slots.
 *
 * AN ARGUMENT WHOSE ADDRESS IS 0 GETS NO VALUE WRITTEN. Its address slot is still
 * written (with 0) and NUMPA still counts it, but its VALUE slot is left as
 * SINTRAN staged it - Nd500MicrocodeServicer.cs:4547-4550. An address of 0 means
 * "this slot carries no operand", and on the swapper's MON 377B the value slot of
 * argument 2 is SINTRAN's own SWPINFO word.
 *
 * @param sv            The servicer.
 * @param x5cpu         Which process is calling, zero-based.
 * @param saved_p       Where the process resumes - AFTER the call instruction,
 *                      unlike a retryable trap, which resumes on it.
 * @param mon_number    The monitor call number.
 * @param arg_count     How many arguments; clamped to NDBUS_MON_MAX_ARGS.
 * @param arg_addresses One address per argument, or NULL for all zero.
 * @param arg_values    One value per argument, or NULL for all zero.
 * @return true once the record is written and the message answered; false when no
 *         message is recorded for that X5CPU, which is counted and logged.
 */
bool ndbus_servicer_answer_monitor_call(NdbusServicer *sv, uint16_t x5cpu, uint32_t saved_p,
                                        uint16_t mon_number, uint32_t arg_count,
                                        const uint32_t *arg_addresses,
                                        const uint32_t *arg_values);

/**
 * Does the microcode inline-copy this monitor call's user buffer?
 *
 * One table, so the behaviour and the thing reported about the behaviour cannot
 * drift apart. The set is {504B, 511B, 512B} and nothing else; see the constants
 * above for why 513B is not in it.
 *
 * @param mon_number The monitor call number.
 * @return true when the buffer must be copied into the message before stopping.
 */
bool ndbus_mon_requires_inline_copy(uint16_t mon_number);

/**
 * Where an inline buffer must be written: the pool byte address ABUFA names.
 *
 * ABUFA HOLDS A POINTER - IT IS NOT THE BUFFER. MP-P2-N500.NPL:140675 reads it and
 * keeps what it finds as N100A, an ND-100 PHYSICAL address:
 *     *AAX ABUFA-N500A; LDDTX; AAX N100A-ABUFA; STDTX  % ND-100 PHYSICAL ADDR
 * LDDTX loads FROM the slot. Writing the text into the message at 2*140B instead
 * made the console print NUL bytes - SINTRAN was reading the real buffer, which
 * was still zero, and ignoring the slot area.
 *
 * AND IT IS A WORD ADDRESS, not a flat byte address. The copy family's addrA/addrB
 * really are flat byte addresses in the window; ABUFA does not come from there, and
 * the ND-100 addresses a word. MEASURED, one run: ABUFA read back as 0x00216800,
 * and 0x216800 << 1 = 0x0042D000, which is one of the buffers SINTRAN itself uses
 * in the same run (0x42CC00 / 0x42D000 / 0x42D400, beside message bases 0x428D30
 * and 0x428E30). Treating it as a flat offset lands two megabytes outside the
 * window, and the program prints whatever stale bytes are at the real buffer.
 *
 * AND IT IS AN ND-100 PHYSICAL ADDRESS, so the window's ND-100 base comes off
 * before it is a pool offset. The 0x42D000 above is ND-100 byte 0x420000 + 0xD000;
 * this servicer's message bases are pool-relative (0x008D30 / 0x008E30), and the
 * same buffer is pool 0x00D000. Used unconverted it named pool 0x42D000: MEASURED
 * 05-OCT-2026, CPU-STAT's 32 output texts were written there while SINTRAN read
 * pool 0x00D000 and printed stale bytes.
 *
 * @param sv       The servicer.
 * @param msg_byte Pool byte offset of the message.
 * @return The pool byte offset of the buffer, or 0 when ABUFA is zero or names
 *         an address below the shared window - no buffer the servicer can
 *         reach, so nothing may be written.
 */
uint32_t ndbus_servicer_inline_buffer_target(const NdbusServicer *sv, uint32_t msg_byte);

/**
 * Copy a monitor call's inline user buffer into the message and flag it.
 *
 * Both guards are the MICROCODE'S, read out of the copy routine at 010662:
 *     010716  count == 0      -> copy nothing
 *     010717  count > 0o4000  -> copy nothing (NOT a truncated prefix)
 * On success MIFLAG bit WSMC is SET explicitly rather than trusted - the bit
 * happening to be set already is exactly what made this fail silently.
 *
 * Exposed so a test can drive it with no CPU attached.
 *
 * @param sv        The servicer.
 * @param msg_byte  Pool byte offset of the message.
 * @param source    The bytes to place, already read from ND-500 memory.
 * @param count     How many; outside 1..NDBUS_MON_INLINE_MAX_BYTES copies nothing.
 * @return true when the bytes were written and WSMC set.
 */
bool ndbus_servicer_write_inline_buffer(NdbusServicer *sv, uint32_t msg_byte,
                                        const uint8_t *source, uint32_t count);

/**
 * Does this process's parked message carry a MONITOR-CALL answer?
 *
 * The restart's answer slots are a union whose arm follows the message kind, so
 * a caller must ask this before reading them. True only when the process last
 * stopped for a monitor call; false after a trap stop, and false when it has not
 * stopped at all.
 *
 * @param sv    The servicer. Not const: an X5CPU at or above
 *              NDBUS_SERVICER_MAX_PROCESSES is counted in x5cpu_out_of_range and
 *              logged once.
 * @param x5cpu The process, zero-based.
 * @return true when NdbusMonResult may be read for that process; false for a
 *         NULL servicer or an out-of-range X5CPU.
 */
bool ndbus_servicer_stop_was_monitor_call(NdbusServicer *sv, uint16_t x5cpu);

/** What SINTRAN sent back with a 3MONCO restart. */
typedef struct NdbusMonResult
{
    /** FUNCV, the call's 32-bit result, which goes into the process's I1.
     *  Carried in the MCNO and MSWMC slots, RE-USED for the answer. */
    uint32_t funcv;

    /** KFLIP, in the STOPR slot, also re-used. Non-zero sets the process's K flag,
     *  which is the ND-500 error convention: the program branches on K. */
    uint16_t kflip;

    /** NUMPA, re-used again - as a WRITE-BACK MASK rather than a count. Bit k set
     *  means parameter k's value must be written into process memory. */
    uint32_t mask;

    /** The write-backs the mask selected, already filtered. */
    uint32_t count;
    uint32_t addresses[NDBUS_MON_MAX_ARGS];
    uint32_t values[NDBUS_MON_MAX_ARGS];

} NdbusMonResult;

/** WHERE A 26B ANSWER-DATA BLOCK IS, not the data itself.
 *
 * A 3WMONCO is the 24B restart plus a bounded copy of answer data into the
 * process's memory before it resumes. This names the copy; nothing is buffered.
 *
 * The data is NOT carried in NdbusMonResult. An earlier version put a 0x1FFF-byte
 * array in there, which put 8 KB on the stack of every monitor-call restart -
 * including the ordinary 24B ones, which have no block at all - and then copied
 * the bytes a second and third time on the way to memory. The reference keeps the
 * block as arguments to its own restart callback and allocates only on this rare
 * path; describing it and streaming it is the same division with no allocation at
 * all.
 *
 * `src_byte` is in the ND-100's half of the pool, which the servicer owns.
 * `dest` is a process LOGICAL address, which only the embedding can translate -
 * the same split the parameter write-backs already use. */
typedef struct NdbusWmoncoBlock
{
    uint32_t dest;      /**< 26ADD: process logical address to copy into */
    uint32_t count;     /**< bytes to copy; 0 when there is nothing, or on oversize */
    uint32_t src_byte;  /**< pool byte offset of the source: (ABUFA << 1) minus the
                             window's ND-100 base - ABUFA is an ND-100 address */
    bool     oversize;  /**< 26NRB >= 0x2000: skip the copy, but STILL resume */
} NdbusWmoncoBlock;

/**
 * @brief Locate the answer-data block of a 26B (3WMONCO) restart.
 *
 * Offsets from the microcode at 015752-016004: 26NRB at halfword 0o17, 26ADD at
 * 0o15-0o16 high halfword first, and ABUFA - a WORD address - at 0o140-0o141,
 * shifted left to reach bytes.
 *
 * THE OVERSIZE CASE IS NOT AN ERROR ANSWER. A count of 0x2000 or more sets
 * `oversize` with `count` 0: the copy is skipped and the process STILL resumes,
 * with FUNCV forced to 0o174 and K set. Refusing the message instead leaves the
 * process parked for ever, which is the defect this whole arm exists to avoid.
 *
 * @param sv       Servicer.
 * @param msg_byte Byte address of the message block; 0 is rejected.
 * @param out      Receives the description; zeroed first.
 * @return true when out was filled, false on a bad argument.
 */
bool ndbus_servicer_read_wmonco_block(NdbusServicer *sv, uint32_t msg_byte,
                                      NdbusWmoncoBlock *out);

/**
 * @brief Read one byte of the ND-100's half of the pool, at a BYTE address.
 *
 * The ND-100 side is word-addressed, so a byte comes out of the halfword that
 * contains it: the even byte is the high half on both machines. This is the one
 * place that extraction lives, so a caller streaming a block cannot get the order
 * wrong in a second copy of the same two lines.
 *
 * @param sv        Servicer.
 * @param byte_addr Pool byte address.
 * @return The byte, or 0 for a bad argument or an address outside the pool.
 */
uint8_t ndbus_servicer_read_nd100_byte(const NdbusServicer *sv, uint32_t byte_addr);

/**
 * @brief Read the answer SINTRAN placed in a message for a 3MONCO restart.
 *
 * THREE SLOTS ARE RE-USED FOR THE ANSWER, and reading them as their outbound
 * meanings gets all three wrong. Decoded from the B30 write-back loop MSG_CONMC
 * 015734-015751: FUNCV is the 32-bit value in the MCNO and MSWMC slots and goes to
 * the process's X1/I1 (015721 D,X1); KFLIP sits in the STOPR slot and sets or
 * clears the K flag (015727/015731 K,ZRO / K,ONE); and NUMPA is a BITMASK, not the
 * count it was on the way out - for each set bit k the 32-bit value at 0o100 + 2k
 * is written into PROCESS memory at the 32-bit address at 0o40 + 2k.
 *
 * THE WRITE-BACK IS NOT OPTIONAL. Without it the program never sees its result:
 * measured 30-SEP-2026, the swapper's MON 377B was reported, SINTRAN answered it,
 * the process resumed from live registers and then spun forever, because the cell
 * it was waiting on had been written in the message and never carried across.
 *
 * This function only READS. Writing into process memory needs the process's MMU,
 * which belongs to the CPU and not to the station, so the caller applies the list.
 *
 * @param sv       The servicer.
 * @param msg_byte The message carrying the answer.
 * @param out      Receives the answer. Must not be NULL.
 * @return true when the message was read.
 */
bool ndbus_servicer_read_monitor_result(NdbusServicer *sv, uint32_t msg_byte,
                                       NdbusMonResult *out);

/**
 * @brief Tell the servicer where the ND-500 physical segment table is.
 *
 * Needed only by the segment-relative members of the copy family, PHYSRD and
 * PHYSWR, whose message carries a physical segment in MSWMC and an offset inside
 * it rather than a flat address. Without it those two fall back to treating the
 * offset as flat, which is logged and counted because a transfer that lands
 * somewhere unintended must not look like a successful one.
 *
 * NO CALLER EXISTS YET - see the pst_base field for why that matches the
 * reference rather than falling short of it.
 *
 * @param sv        The servicer.
 * @param pst_byte  Pool byte offset of the table, or 0 to clear it.
 * @return true on success; false when sv is NULL.
 */
/**
 * @brief The X5CPU - the PROCESS number - carried by a message block.
 *
 * X5CPU names the process, not the station: 0 is the swapper and 1 is the first
 * domain, and from PLACE-DOMAIN onward both are live at once with a message block
 * each. It must be read from the block that is being served. The station's own
 * index on the octobus (ndbus_cpu_context_x5cpu(), derived from the station
 * number) is a DIFFERENT quantity and is 0 for every process on a single ND-5000,
 * so using it here routes every process's trap onto the swapper's message.
 *
 * Mirrors RetroCore's servicer.ReadMessageX5Cpu(msgByteAddress), which its process
 * bridge calls for exactly this decision.
 *
 * @param sv       The servicer; its pool supplies the bytes.
 * @param msg_byte Pool-relative byte address of the message block.
 * @return The X5CPU field, or -1 if the servicer, its pool or the address is unusable.
 */
/**
 * @brief Pool-relative byte address of one process's context block.
 *
 * The one formula for it: the context area base, then one stride for the area
 * header, then one stride per process. ndbus_servicer_process_message() already
 * computed this inline for the start it was serving; a context SWITCH needs the
 * block of a process no message names, so the formula is named here instead of
 * repeated at the second caller.
 *
 * Mirrors RetroCore's servicer.GetProcessContextAddress(x5cpu), which its process
 * bridge calls on both sides of a context switch.
 *
 * @param sv    The servicer.
 * @param x5cpu The PROCESS number - see ndbus_servicer_read_message_x5cpu(). NOT
 *              range checked against NDBUS_SERVICER_MAX_PROCESSES: the reference
 *              computes the address for any X5CPU
 *              (Nd500MicrocodeServicer.cs:1326-1336 and :3744-3745), so a block
 *              that lies outside the pool is the caller's to refuse.
 * @return The block's byte address, or 0 when no context area has been declared
 *         yet. Zero is not a legal block address, so a caller that ignores the
 *         check gets a refusal from ndbus_context_attach() rather than block 0 of
 *         the pool.
 */
uint32_t ndbus_servicer_process_context_byte(const NdbusServicer *sv, uint16_t x5cpu);

int ndbus_servicer_read_message_x5cpu(const NdbusServicer *sv, uint32_t msg_byte);

bool ndbus_servicer_set_pst_base(NdbusServicer *sv, uint32_t pst_byte);

/**
 * @brief Tell the servicer where the context (register) block area starts.
 *
 * @param sv        The servicer.
 * @param area_byte Pool byte offset of the area, or 0 to clear it.
 * @return true on success; false when sv is NULL.
 */
bool ndbus_servicer_set_context_area(NdbusServicer *sv, uint32_t area_byte);

/**
 * @brief Set what 3RMICV reports, when the loaded control store says.
 *
 * @param sv            The servicer.
 * @param micro_version The microprogram version, from control-store word 1's LARG
 *                      halfword. Ignored when 0 - an unloaded or unpatched store -
 *                      so the default is kept rather than a zero reported.
 * @param cpu_parameter The per-model CPU parameter. Ignored when 0, for the same
 *                      reason: there is no verified mapping for every model byte
 *                      and a wrong image must be visible, not silently relabelled.
 * @return true on success; false when sv is NULL.
 */
bool ndbus_servicer_set_cpu_identity(NdbusServicer *sv, uint16_t micro_version,
                                     uint16_t cpu_parameter);

/**
 * @brief Host bytes per unit of an ND-100-side address operand on this transport.
 *
 * SINTRAN's CNVWADR emits a transport-specific quantity: a physical WORD address on
 * the ND-500 3022 (2 bytes per unit), a BYTE offset inside the 5MPM window on the
 * ND-5000 octobus (1 byte per unit). This servicer copies over a single flat,
 * byte-addressed window - see the copy-family comment at ndbus_servicer.c:238 and
 * resolve_physical_segment - so its convention is the byte-offset one.
 *
 * Exists so the CPU's RIOM mapping is DERIVED from the servicer that defines the
 * convention instead of restating it: one source of truth, so the copy engine and
 * RIOM cannot drift apart.
 *
 * @return 1. Constant for this servicer, and a function rather than a macro so a
 *         future transport is taught here once.
 */
uint32_t ndbus_servicer_nd100_bytes_per_unit(void);

#endif /* NDBUS_SERVICER_H */
