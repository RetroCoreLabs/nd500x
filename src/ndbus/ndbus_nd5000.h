/**
 * @file ndbus_nd5000.h
 * @brief The ND-5000's octobus station, and the shared-memory doorbell.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * One of these sits at each occupied ND-5000 slot, octal 70B..76B (decimal
 * 56..62) - ND-05.020.01 T329. It is the ACCP as the bus sees it: the ND-120
 * sends multibyte messages to OMD 3, this reassembles them, runs the guard table
 * in ndbus_accp.h and answers Messack or Messnak.
 *
 * IT ALSO SERVES OMD 0, the Octobus Test Protocol of ndbus_testproto.h, which is a
 * SEPARATE protocol with its own commands, its own reply header and its own magic
 * word. ND-05.020.01 ~3930 reserves OMD 0 for the octobus test programs and the
 * ACCP firmware answers them; the microprogram never sees that traffic. The two
 * paths are dispatched by the OMD number in the multibyte frames and share only
 * the envelope, so nothing about OMD 0 changes what OMD 3 does.
 *
 * WHAT TRAVELS WHERE. T99: "The octobus is used for messages to initiate
 * operations. As a general rule, these operations work on data in shared memory
 * in the MFbus system. Thus, the octobus is normally not used to transport data.
 * The only exception is during debugging and testing." So the octobus carries
 * the command; the DATA is in the pool, and the doorbell below is how the two
 * sides tell each other it is there.
 */

#ifndef NDBUS_ND5000_H
#define NDBUS_ND5000_H

#include "ndbus_accp.h"
#include "ndbus_mailbox.h"
#include "ndbus_multibyte.h"
#include "ndbus_octobus.h"
#include "ndbus_pool.h"
#include "ndbus_servicer.h"
#include "ndbus_testproto.h"

/**
 * @brief Latch the doorbell on the FIRST matching transition.
 *
 * THE X5ACT DOORBELL, AND THE THRESHOLD THAT CANNOT BE MET.
 *
 * Self-discovery watches shared memory for a cell that goes 0xFFFF -> 0, which
 * is the signature of the guest ringing the doorbell.
 *
 * `[V-SRC]` Per the byte-verified microcode reference, XMSINIT initialises X5ACT
 * to -1, and the microcode re-arms it by writing **1**, not -1 - microword
 * 0o24722's literal 1 at IDLE_2, which runs BEFORE the message is consumed. So
 * the genuine doorbell produces exactly ONE -1 to 0 transition per XMSINIT;
 * every ring after that is 1 -> 0 and does not match the signature.
 *
 * A REPEAT THRESHOLD OF 2 OR MORE THEREFORE CANNOT BE MET. The sniff never
 * latches, and NOTHING ERRORS - the machine simply sits looking idle. That is
 * the worst shape a setting can have: it does not fail, it quietly answers a
 * different question. The threshold is kept and kept settable, because
 * suppressing self-discovery on purpose is a legitimate thing to want while
 * chasing a mis-latch; setting 2 or more logs the warning above at the moment it
 * is set, rather than leaving someone to work it out from an idle machine.
 *
 * The older claim that the real doorbell repeats because a watchdog rings it was
 * reasoning about what a watchdog ought to do, never an observation.
 */
#define NDBUS_X5ACT_LATCH_ON_FIRST 1u

/** @brief State of the shared-memory doorbell sniff. */
typedef struct NdbusDoorbellSniff
{
    uint32_t candidate_offset; /**< pool offset being watched */
    uint32_t transitions;      /**< 0xFFFF -> 0 transitions seen at it */
    uint32_t threshold;        /**< 0 or 1 = latch on the first (see above) */
    bool     latched;          /**< true once the doorbell has been identified */
    bool     have_candidate;   /**< true once an offset is being watched */
} NdbusDoorbellSniff;

typedef struct NdbusNd5000 NdbusNd5000;

/** @brief Kick numbers a kick frame can carry: bits 5-0 of the frame, 0..63. */
#define NDBUS_ND5000_KICK_NUMBERS 64u

/**
 * @brief The 32-bit word SINTRAN writes into the parameter area before VPARP.
 *
 * Ported from OctobusND5000Station.cs VparpTestPattern (line 3750), whose remark
 * gives the carve: the CS-load sequencer in segment 030-S3SM5 loads
 * A:D = 062626B:115511B, calls VPARP and compares the echo against the same pair,
 * so the wire value is 0x6596 9B49, high word first. The ADRZERO calibration
 * looks for exactly this word.
 */
#define NDBUS_ACCP_VPARP_TEST_PATTERN 0x65969B49u

/** @brief One ND-5000 as the octobus sees it: its ACCP station plus its state. */
struct NdbusNd5000
{
    NdbusStation        station;   /**< registered on the fabric; MUST be first */
    NdbusPool          *pool;      /**< the shared MFbus memory */
    const NdbusHostOps *host;      /**< host callbacks for logging; may be NULL */
    const NdbusCpuOps  *cpu;       /**< the ND-5000 this station fronts; may be NULL */

    NdbusAccpState      accp;      /**< the ACCP guard state, see ndbus_accp.h */
    NdbusMultibyte      inbox;     /**< the OMD-0 or OMD-3 message being reassembled */
    /** The OMD-0 Test Protocol responder. One collector serves both OMDs - the
     * EOMB frame says which protocol the completed message belongs to, exactly as
     * RetroCore OctobusND5000Station.cs HandleFrame does - so a station cannot be
     * mid-message on both at once, which is also true of the real card: there is
     * one octobus receiver. */
    NdbusTestProto      testproto;
    NdbusDoorbellSniff  sniff;     /**< the shared-memory doorbell sniff */

    /** LPARP's 4-byte pointer to the parameter area in MFbus memory, exactly as
     * it arrived: a BYTE offset from ND-500 physical address zero (ADRZERO), NOT
     * from pool byte 0. VPARP, LOCSM and DUCS turn it into a pool offset through
     * `nd500_zero_byte` below. VPARP reads the 32-bit word there and echoes it
     * back - the one ACCP command a canned Messack cannot satisfy. */
    uint32_t            parameter_pointer;

    /**
     * ND-100 physical BYTE address of ND-500 physical address 0 - what SINTRAN
     * calls ADRZERO - or 0 while it has not been calibrated, in which case the
     * shared window's own ND-100 base (servicer.nd100_window_base_byte) is used.
     *
     * Ported from $RETROCORE/Emulated.HW/ND/CPU/NDBUS/OctobusND5000Station.cs
     * field _nd500ZeroByte (line 252) and property Nd500ZeroByte (line 258).
     * IT IS NOT A CONSTANT AND IT IS NOT THE WINDOW BASE: that file records the
     * same pack reporting "ND-500 address zero" as page 2112 on a 2 MB ND-100 and
     * page 4096 on a 4 MB one. Nothing in the ACCP command stream announces it,
     * so VPARP works it out from the test word SINTRAN wrote - CalibrateNd500Zero,
     * line 3885.
     *
     * Kept as an ND-100 address, not as a pool offset, because the calibration
     * rule "ADRZERO is an ND-100 PAGE number, so the base is 2048-byte aligned"
     * is a statement about the ND-100 address.
     */
    uint32_t            nd500_zero_byte;

    /** The control store the CS-load pulses fill, as halfwords, lazily
     * allocated on the first LOCSM pulse and NULL until then; see
     * NDBUS_CS_WORD_COUNT. Freed by ndbus_nd5000_destroy(). */
    uint16_t           *control_store;

    /* CS-load diagnostics, so a test can say what the load did rather than
     * infer it from the pool. */
    unsigned long       cs_load_pulses;   /**< LOCSM pulses serviced */
    unsigned long       cs_dump_pulses;   /**< DUCS pulses serviced */
    uint32_t            cs_last_dump_words; /**< microword halfwords the last DUCS served */
    uint16_t            cs_last_addend;   /**< checksum addend the last DUCS wrote */

    /** The mailbox this station polls, derived from the control store at ENKICK -
     * see ndbus_nd5000_mailbox_from_control_store(). Unattached until then. */
    NdbusMailbox        mailbox;
    /** START_MESS as read out of the control store: the mailbox header's byte
     * offset in the pool. 0 while the control store has not been patched. */
    uint32_t            start_mess;
    /** SAMSON_CPU as read out of the control store: this CPU's extension-block
     * index in the mailbox. */
    uint32_t            samson_cpu;
    /**
     * The mailbox CPUNO used when the SAMSON_CPU cell reads zero. 1 from init,
     * and afterwards the CPUNO of the last mailbox configuration.
     *
     * Ported from OctobusND5000Station.cs field _cpuNumber (line 949, "= 1") as
     * ConfigureMailbox (line 1398) updates it and ConfigureMailboxFromControlStore
     * (line 1453) falls back to it. It is NOT derived from the station number:
     * the reference uses 1 for a station at 071B too.
     */
    int                 cpu_number;

    /** Pool byte offset of the ND-500 physical segment table, from patched
     *  control-store cell 0o21 (a PAGE there, shifted to bytes). The reference's
     *  LoadedPstp. 0 until a 066B/035B/036B microprogram start reads a patched
     *  cell. */
    uint32_t            pst_base;
    /** Pool byte offset of the context (register) block area, from patched
     *  control-store cell 0o20, which is already a byte address. The reference's
     *  LoadedContextBlockBase. 0 until a 066B/035B/036B microprogram start, or
     *  ndbus_nd5000_try_get_context_block_area_base(), reads a patched cell. */
    uint32_t            context_area;

    /** The mailbox servicer: the ND-5000 microprogram's side of the 5MPM
     * mailbox. Walks the X5BEX chain, answers the MICFU and inserts into the
     * X5FIF ring. See ndbus_servicer.h. */
    NdbusServicer       servicer;

    /**
     * LSYSPAR's word 1 - host station and OMD - kept because the GIVEINT answer
     * interrupt is COMPOSED FROM IT and from nothing else:
     *
     *     interrupt word = (LSYSPAR word 1 AND 0x3F00) OR 0x8001
     *
     * which is microcode GIVEINT1 at 025440-025441, raw:
     *     025440   SC12 := SARG(0x3F00) AND SC10
     *     025441   SC12 := SARG(0x8001) OR SC12  -> ACCP_WRITE
     * There is NO shift in those two words. RetroCore carried a ">> 3" here from
     * AUG-2026 until 09-SEP-2026, and it was only compensating for LSYSPAR word 1
     * being parsed one byte late elsewhere; with the ident byte in place the plain
     * expression yields 100401B = 0x8101, the value observed on the live machine.
     * Ported from OctobusND5000Station.cs IServicerHost.AnswerWritten.
     *
     * 0 until LSYSPAR arrives, and a 0 composes to 0x8001 - destination station 0,
     * which the fabric drops. The counters below say when that happened rather
     * than leaving it silent.
     */
    uint16_t            lsyspar_word1;

    /* Mailbox-servicing diagnostics. */
    unsigned long       service_polls;      /**< ndbus_nd5000_service_mailbox() calls */
    unsigned long       service_activations;/**< polls that found X5ACT rung */
    unsigned long       giveint_frames;     /**< GIVEINT answer frames handed to the fabric */
    unsigned long       giveint_no_fabric;  /**< answers with no fabric attached */
    unsigned long       giveint_no_mailbox; /**< answers with no mailbox located */
    uint16_t            last_giveint_frame; /**< the last frame word composed */
    uint16_t            system_parameters[3]; /**< the three LSYSPAR words, S5/S6/S7 */
    unsigned long       model_reports;      /**< ENKICK model/version reports sent */
    unsigned long       report_no_store;    /**< reports skipped: no control store */
    uint8_t             last_model_report_model;   /**< CPUMODEL byte last reported */
    uint16_t            last_model_report_version; /**< LARG version last reported */

    /* Diagnostics, so a test can say what the station did rather than infer it. */
    unsigned long       messages_handled; /**< complete OMD-3 messages handled */
    unsigned long       messacks;         /**< Messack replies sent */
    unsigned long       messnaks;         /**< Messnak replies sent */
    uint8_t             last_command;     /**< command byte of the last message */
    int                 last_nak_code;    /**< NDBUS_ACCP_ACCEPTED when the last was a Messack */

    /**
     * True while the ACCP program sits in its idle loop, which is where a
     * terminate (244B) puts it. A master clear (241B), a continue (242B) and a
     * microprogram start (066B, 035B, 036B) take it out again.
     *
     * WHILE IT IS SET THE STATION DROPS EVERY FRAME except an emergency, a
     * SOMB/EOMB for OMD 0 or OMD 3, and the data bytes of a message those opened.
     * A terminated ACCP still answers ACCP command messages - only the
     * microprogram is stopped - so this gates kicks and microprogram traffic, not
     * commands. Ported from OctobusND5000Station.cs HandleFrame, lines 2383-2397.
     */
    bool                accp_idle;

    /* Emergency counters, so a test can say what arrived rather than infer it. */
    unsigned long       master_clears;    /**< 241B emergency frames handled */
    unsigned long       continues;        /**< 242B emergency frames handled */
    unsigned long       terminates;       /**< 244B emergency frames handled */
    uint8_t             last_emergency;   /**< information byte of the last one */

    /**
     * Every kick frame this station has decoded, indexed by kick number 0-63,
     * counted whether or not kicks were enabled. A broadcast kick is rejected
     * before it is counted, and a kick that arrives while the ACCP is idle never
     * reaches the decoder. Ported from OctobusND5000Station.cs KickCounts (line
     * 563), which exists because "I saw no log line" is not evidence that a kick
     * never arrived.
     */
    unsigned long       kick_counts[NDBUS_ND5000_KICK_NUMBERS];
    /** Kick frames discarded because ENKICK had not been issued. The reference's
     *  KicksDroppedDisabled (line 566). */
    unsigned long       kicks_dropped_disabled;

    /**
     * The ND-5000 CPU behind this station, as two optional callbacks. The station
     * holds no CPU pointer of its own, so the embedding that owns the CPU installs
     * these with ndbus_nd5000_set_cpu_hooks(). Either may be NULL, which is the
     * reference's "_cpu == null": the station then changes only its own state.
     */
    void (*reset_cpu_to_idle)(void *ctx);  /**< C# ResetCpuToIdle, line 2652 */
    void (*apply_init_state)(void *ctx);   /**< C# CpuND500.ApplyND5000InitState, called at line 3448 */
    void               *cpu_hook_ctx;      /**< handed back to both callbacks */
};

/* ---- the control store, and the load that has to check out -------------------
 *
 * ND-5000 microwords are 128 bits, which the ACCP moves as EIGHT 16-bit
 * halfwords, slice 0 being bits 127-112. The store is 16384 microwords, so a
 * full load is 256 KB - measured on the live machine, 30-SEP-2026: SINTRAN's
 * ND-500/5000 monitor J04 sends 128 LOCSM pulses of N=128 microwords each,
 * control-store addresses 0x0000, 0x0080, ... 0x3F80, which is exactly 16384.
 *
 * NO MICROCODE IS EXECUTED FROM THIS BUFFER. It exists so the ND-100-side
 * read-back checksum has something true to read back: the monitor follows the
 * 128 write pulses with ONE DUCS pulse (N=1, control-store address 0), sums the
 * eight halfwords it gets back and compares them against an addend word, and
 * reports "Checksum error" when they disagree. Ported from RetroCore
 * $RETROCORE/Emulated.HW/ND/CPU/NDBUS/OctobusND5000Station.cs, ServiceControlStoreWrite
 * (line 4007) and ServiceControlStoreReadback (line 4119).
 */

/** @brief Microwords the control store holds (16384, i.e. 256 KB of 128-bit words). */
#define NDBUS_CS_WORD_COUNT 16384u
/** @brief 16-bit halfwords per 128-bit microword, slice 0 being bits 127-112. */
#define NDBUS_CS_HALFWORDS_PER_WORD 8u
/** @brief Halfwords in the whole control store. */
#define NDBUS_CS_HALFWORDS (NDBUS_CS_WORD_COUNT * NDBUS_CS_HALFWORDS_PER_WORD)

/** @brief Emergency 241B: master clear - resets the ACCP and the ND-5000 CPU. */
#define NDBUS_EMERGENCY_MASTER_CLEAR   0xA1u
/** @brief Emergency 242B: continue ACCP - leave the idle loop and start up. */
#define NDBUS_EMERGENCY_CONTINUE_ACCP  0xA2u
/** @brief Emergency 244B: terminate ACCP - enter the idle loop, stop the microprogram. */
#define NDBUS_EMERGENCY_TERMINATE_ACCP 0xA4u

/**
 * @brief Bring one ND-5000 station up at `station_number`.
 *
 * Does NOT register on a fabric; call ndbus_fabric_register(&nd->station).
 *
 * @param nd             The station to initialise.
 * @param station_number The octobus station, which must be 70B..76B (decimal
 *                       56..62).
 * @param pool           The shared MFbus memory.
 * @param host           Host callbacks for logging; may be NULL.
 * @param cpu            The ND-5000 this station fronts; may be NULL.
 * @return true when initialised. false for a station number outside 70B..76B -
 *         a station number is configuration, and an ND-5000 answering at, say,
 *         10B would collide with a SCSI controller rather than fail visibly.
 */
bool ndbus_nd5000_init(NdbusNd5000 *nd, uint8_t station_number, NdbusPool *pool,
                       const NdbusHostOps *host, const NdbusCpuOps *cpu);

/**
 * @brief Put the station's own ACCP state back to cold: the four guard cells, the
 *        parameter pointer, the message being collected and the doorbell sniff.
 *
 * THIS IS NOT WHAT THE ACCP COMMAND 071B CPURES DOES, and the command no longer
 * calls it. The reference clears exactly two cells on CPURES - "microprogram
 * running" and "kicks enabled" - and leaves the system parameters and the
 * parameter pointer given (OctobusND5000Station.cs lines 3151-3168, the second
 * one measured on the real firmware as cell 0x1143B6 going 0001 -> 0000). This
 * function clears more than that, so it is a helper for an owner that wants a
 * cold station, not a model of any octobus command.
 *
 * The pool is NOT touched - it is shared memory and the ND-100 owns its
 * contents. The CPU hooks, the mailbox and the control store are kept.
 *
 * @param nd The station to reset.
 * @return Nothing. Cannot fail.
 */
void ndbus_nd5000_reset(NdbusNd5000 *nd);

/**
 * @brief Install the two callbacks that reach the ND-5000 CPU behind the station.
 *
 * The station calls them at exactly the points the reference touches its CPU:
 *
 *   reset_cpu_to_idle   emergency 241B MASTER CLEAR, after the station's own
 *                       state is reset (OctobusND5000Station.cs line 2675), and
 *                       command 071B CPURES, before the two guard cells are
 *                       cleared (line 3157). The implementation must do what
 *                       ResetCpuToIdle does (lines 2652-2663): reset the CPU AND
 *                       park it again in the microcode IDLE state. A reset alone
 *                       leaves the CPU runnable at PC 0, which that file records
 *                       as a runaway CPU during a full SINTRAN boot.
 *
 *   apply_init_state    command 066B STAMIC0 only - not 035B or 036B - after the
 *                       patched control-store cells have been read and before the
 *                       acknowledge (lines 3447-3448). The implementation must do
 *                       what CpuND500.ApplyND5000InitState does.
 *
 * Call it AFTER ndbus_nd5000_init(), which clears the station.
 *
 * @param nd                The station.
 * @param reset_cpu_to_idle The reset-and-park callback, or NULL for none.
 * @param apply_init_state  The post-INIT state callback, or NULL for none.
 * @param ctx               Handed back to both callbacks unchanged.
 * @return true on success; false when nd is NULL.
 */
bool ndbus_nd5000_set_cpu_hooks(NdbusNd5000 *nd, void (*reset_cpu_to_idle)(void *ctx),
                                void (*apply_init_state)(void *ctx), void *ctx);

/**
 * @brief Read the context block area base out of patched control-store cell 0o20,
 *        now, whatever was or was not read at microprogram start.
 *
 * Ported from OctobusND5000Station.cs IServicerHost.TryGetContextBlockAreaBase
 * (lines 2181-2189). The reference's servicer calls it wherever it needs the base
 * and its cached value is still zero, so a cell that was read too early - or a
 * start path that never ran - cannot leave a process start resolving against a
 * placeholder. Also records the value in `context_area` when that is still 0.
 *
 * NOT YET CALLED BY ndbus_servicer.c, which has no host callback for it; see the
 * note beside the definition.
 *
 * @param nd        The station.
 * @param byte_base Receives the cell's 32-bit LARG value, 0 when there is no
 *                  control store. Must not be NULL.
 * @return true when the cell holds a non-zero value; false when nd or byte_base
 *         is NULL, when no control store is loaded, or when the cell is still the
 *         unpatched zero.
 */
bool ndbus_nd5000_try_get_context_block_area_base(NdbusNd5000 *nd, uint32_t *byte_base);

/**
 * @brief Release what the station allocated, leaving it zeroed.
 *
 * The POOL is not touched - it is shared memory the ND-100 owns, and several
 * stations look into the same one. Only the station's own control-store buffer
 * is freed.
 *
 * @param nd The station to release. NULL is ignored, and a second call on the
 *           same station is safe.
 * @return Nothing. Cannot fail.
 */
void ndbus_nd5000_destroy(NdbusNd5000 *nd);

/**
 * @brief Derive the mailbox base from the loaded control store, as ENKICK does.
 *
 * SINTRAN PATCHES THE ANSWER INTO THE MICROCODE BEFORE IT BURNS IT. Two cells of
 * the control store carry it, and they are LARG constants, which are 32-bit
 * fields built from halfwords 6 and 7 - not single halfwords:
 *
 *     word 026B (0x16)  START_MESS  the mailbox header's BYTE offset in the pool
 *     word 025B (0x15)  SAMSON_CPU  this CPU's extension-block index
 *
 * Ported from RetroCore
 * $RETROCORE/Emulated.HW/ND/CPU/NDBUS/OctobusND5000Station.cs,
 * ConfigureMailboxFromControlStore, including the reason the field is read as 32
 * bits: a pack that places the mailbox past window offset 0xFFFF truncates to a
 * wrong base if only halfword 7 is read, and the microcode then polls the wrong
 * cell for ever.
 *
 * MEASURED on the live machine 30-SEP-2026, ND-500/5000 MONITOR J04 on
 * START-SWAPPER, read out of this station's control store at ENKICK:
 *
 *     cs[0x16] = 4000 0001 DE01 6010 0000 0000 0000 8800   START_MESS = 0x8800
 *     cs[0x15] = 4000 0001 DE01 6010 0000 0000 0000 0001   SAMSON_CPU = 1
 *
 * which is the same pair RetroCore's OctobusCsLoadTests
 * CsLoad_Enkick_DerivesMailboxBaseFromStartMess pins.
 *
 * THIS REPLACES THE 0xFFFF -> 0 DOORBELL SNIFF as the way the mailbox is found.
 * The sniff latches on a transition that a re-armed doorbell never repeats, and
 * it guesses; the control store states the answer.
 *
 * WHICH CPUNO, AND WHAT IS REFUSED - both as the reference has them (lines
 * 1453-1461). SAMSON_CPU is used when it is non-zero; when it is zero the
 * fallback is `cpu_number`, which is 1 from init and NOT the station's position
 * on the bus. The base is refused only when the header address wraps, or when
 * fewer than 17 bytes of the extension block lie inside the pool:
 *     header < window start  ||  extension + 16 >= window start + window size
 * It is NOT required that the whole 256-byte block fits.
 *
 * THIS FUNCTION READS START_MESS AND SAMSON_CPU ONLY. The physical segment table
 * (cell 0o21), the context block area (cell 0o20) and the CPU identity are read
 * at the 066B/035B/036B microprogram start, each on its own and whether or not a
 * mailbox was found - LoadMmsPointersFromControlStore and
 * DeriveCpuIdentityFromControlStore in the reference, called at lines 3396 and
 * 3402.
 *
 * @param nd The station. Its control store must already be loaded - LOCSM fills
 *           it - or there is nothing to read.
 * @return true when a mailbox was configured. false, changing nothing, when the
 *         control store is empty, when START_MESS is still 0 (unpatched), or when
 *         the guard above refuses the base.
 */
bool ndbus_nd5000_mailbox_from_control_store(NdbusNd5000 *nd);

/**
 * @brief Offer one shared-memory word write to the doorbell sniff, BEFORE it lands.
 *
 * CALL ORDER IS PART OF THE CONTRACT, and the name says so because getting it
 * wrong is silent. The signature the sniff looks for is the TRANSITION
 * 0xFFFF -> 0, so the function reads the cell to learn the PREVIOUS value. Call
 * it after the write has already landed and the previous value it reads is the
 * new one - no transition is ever seen, the sniff never latches, and the machine
 * sits looking idle.
 *
 * That is the same failure mode as setting an unreachable repeat threshold,
 * reached by a completely different route, which is why the order is in the name
 * rather than only in a comment.
 *
 * This function does NOT perform the write. The caller still does that.
 *
 * Call it with the pool offset and the value being written. Once latched, the
 * offset is in `sniff.candidate_offset` and `sniff.latched` is set.
 *
 * @param nd     The station whose sniff is being offered the write.
 * @param offset The pool byte offset being written.
 * @param value  The 16-bit value being written there.
 * @return true on the write that LATCHES the doorbell, false on every other
 *         write - including writes that advance the transition count without
 *         reaching the threshold. false is not an error.
 */
bool ndbus_nd5000_sniff_before_write16(NdbusNd5000 *nd, uint32_t offset, uint16_t value);

/**
 * @brief Set the repeat threshold for the doorbell sniff.
 *
 * Logs the warning in the NDBUS_X5ACT_LATCH_ON_FIRST comment above through
 * host->log when a value of 2 or more is set, because such a value can never be
 * met.
 *
 * @param nd        The station.
 * @param threshold The number of 0xFFFF -> 0 transitions required to latch; 0 or
 *                  1 means latch on the first.
 * @return Nothing. A value of 2 or more is accepted, not refused, and is
 *         reported only by that log line.
 */
void ndbus_nd5000_set_sniff_threshold(NdbusNd5000 *nd, uint32_t threshold);

/**
 * @brief One microcode IDLE-loop iteration: poll the doorbell and serve the queue.
 *
 * Ported from RetroCore OctobusND5000Station.cs ServiceMailbox + WalkQueue. The
 * protocol is NOT symmetric and the asymmetry is the whole point: SINTRAN's ACT51
 * writes X5ACT := 0 and sends no kick, so the only way the ND-5000 learns of work
 * is by polling. This function is that poll.
 *
 *   X5ACT != 0   nothing pending. -1 is XMSINIT's idle value, 1 is the re-armed
 *                value, and both mean the same thing here.
 *   X5ACT == 0   rung. Re-arm it to 1 - which is what IDLE_2 writes, NOT -1 -
 *                then walk the X5BEX chain and answer what is ours.
 *
 * X5BEX is a WINDOW-RELATIVE BYTE offset, not an ND-100 word address: the 3022's
 * word << 1 lands outside the window entirely. Live-verified in RetroCore on
 * 21-JUL-2026 with X5BEX = 0xBE30.
 *
 * @param nd The station. Does nothing and returns false when no mailbox has been
 *           located yet - the control store has to be loaded and ENKICK seen
 *           first, see ndbus_nd5000_mailbox_from_control_store().
 * @return true when at least one message was answered by this poll.
 */
bool ndbus_nd5000_service_mailbox(NdbusNd5000 *nd);

/**
 * @brief Install the process host: who starts a process when a start message arrives.
 *
 * The servicer answers a start-class message (22B STARTP0, 23B 3START, 25B 3TRACO)
 * by asking this callback to start the process on the real ND-5000 behind the
 * station. Without one, every start is DECLINED and answered the way a station with
 * no CPU behind it answers - which SINTRAN reports as the swapper having stopped.
 *
 * The callback is handed the station pointer as its context, so an implementation
 * can find its own CPU from the station number.
 *
 * @param nd    The station.
 * @param start The callback, or NULL to remove one. It must return true ONLY when a
 *              CPU really began running: true tells the servicer to leave the
 *              message WAITING and never answer it, so a true with nothing started
 *              leaves SINTRAN waiting for an answer that cannot come.
 * @return true on success; false when nd is NULL.
 */
bool ndbus_nd5000_set_process_host(NdbusNd5000 *nd,
                                   bool (*start)(void *ctx, uint32_t msg_byte, uint16_t micfu,
                                                 uint32_t ctx_byte));

/**
 * @brief Install the callback that declares a learned Domain Information Table base.
 *
 * The servicer learns the base by watching where SINTRAN's trap-config PHYSWR
 * transfers land, and calls this before it offers a start to the process host. The
 * implementation must DECLARE the base only - recording it and marking the table
 * configured - and must not initialise or zero the table, because the guest has
 * already filled it.
 *
 * @param nd      The station.
 * @param declare The callback, or NULL to remove one. Its base argument MAY BE ZERO,
 *                which is a legitimate base and not a "nothing learned" signal.
 * @return true on success; false when nd is NULL.
 */
/**
 * Give the station a way to read ND-500 DATA memory through the MMU.
 *
 * Needed for the inline user buffer of the output monitor calls - see
 * ndbus_mon_requires_inline_copy(). Without it those calls stop with the buffer
 * unwritten and SINTRAN prints whatever stale bytes are at ABUFA.
 *
 * @param nd   The station.
 * @param read The reader, or NULL to remove it.
 * @return false only when nd is NULL.
 */
bool ndbus_nd5000_set_data_reader(NdbusNd5000 *nd,
                                  bool (*read)(void *ctx, uint32_t logical_address,
                                               uint8_t *destination, uint32_t count));

bool ndbus_nd5000_set_dit_declarer(NdbusNd5000 *nd,
                                   void (*declare)(void *ctx, uint32_t base));

#endif /* NDBUS_ND5000_H */
