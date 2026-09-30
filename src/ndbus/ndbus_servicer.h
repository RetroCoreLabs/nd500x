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
#define NDBUS_MICFU_PHYSRD    24u   /**< 30B PHYSRD physical-memory read */
#define NDBUS_MICFU_PHYSWR    25u   /**< 31B PHYSWR physical-memory write */
#define NDBUS_MICFU_IMEMWR    29u   /**< 35B IMEMWR instruction-memory write */
#define NDBUS_MICFU_WREG      17u   /**< 21B 3WREG register write; MSG_ILLEG on the B30 */
#define NDBUS_MICFU_STARTP0   18u   /**< 22B MSG_STARTP0 start process 0 */
#define NDBUS_MICFU_START     19u   /**< 23B 3START start process */

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
#define NDBUS_SERVICER_COPY_LOG_LIMIT 24u

/** How many ND-5000 processes one mailbox can carry, one message remembered each.
 *  The extension blocks run CPUNO 1..7, so 8 covers a zero-based X5CPU. */
#define NDBUS_SERVICER_MAX_PROCESSES 8u

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
    unsigned long copies_refused;      /**< transfers refused for leaving the pool */
    /** DMEMRD/DMEMWR refused because no host can translate a logical data address.
     *  Counted separately from copies_refused: that one is a bad address, this one
     *  is a missing capability, and they need different fixes. */
    unsigned long logical_copies_refused;
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

#endif /* NDBUS_SERVICER_H */
