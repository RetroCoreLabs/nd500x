/*
 * ndbus_nd5000.c - the ND-5000's octobus station
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ndbus_lock.h"
#include "ndbus_nd5000.h"

#include "ndbus_servicer.h"

static void nd_log(const NdbusNd5000 *nd, const char *message)
{
    if (nd == NULL || nd->host == NULL || nd->host->log == NULL)
    {
        return;
    }
    nd->host->log(nd->host->ctx, 0, message);
}

/* ---- the reply --------------------------------------------------------------
 *
 * T125: "In response to a multibyte message, the ACCP sends either Message
 * acknowledged (Messack) or Message not acknowledged (Messnak). The first
 * (Messack) is only a single byte ... The second (Messnak) returns an error code
 * and two bytes of status".
 *
 * Both go back as a multibyte message of their own. This builds the FRAMES; the
 * fabric delivers them.
 */
/*
 * Messack, with any return parameters after it: the leading status byte 0 is
 * CMACK/MFACK, "OK / alive / self-test passed".
 *
 * T125: "If a command requires a response with returned data via octobus (e.g.
 * READ MIR), this data is sent directly after Messack in the same multibyte
 * message. (Messack is sent as a multibyte message even if there are no return
 * parameters.)" `params` / `param_count` are those return bytes; pass 0 for a
 * bare Messack.
 *
 * Ported from RetroCore OctobusND5000Station.cs SendAccpMessack: the payload is
 * an all-zero status WORD, and a command that returns data puts the same 0x00 ack
 * byte in front of its data bytes (SendAccpData). The 68000 self-test is NOT
 * modelled - a status-0 Messack is all SINTRAN observes of it. The control store
 * IS modelled, by accp_load_control_store and accp_dump_control_store below,
 * because SINTRAN sums what it reads back and a canned acknowledge cannot satisfy
 * that sum.
 *
 * reply_omd is the SOURCE OMD out of the command message, never a constant: that
 * is the OMD the sender is listening on.
 */
static int build_messack(uint8_t station, uint8_t reply_omd, const uint8_t *params,
                         int param_count, uint16_t *replies, int max)
{
    uint8_t payload[1 + 32];   /* 0x10 RECO answers with 16 words after the status byte */

    if (param_count < 0 || param_count > (int)sizeof(payload) - 1)
    {
        return 0;
    }

    payload[0] = 0x00; /* MFACK */
    for (int i = 0; i < param_count; i++)
    {
        payload[1 + i] = params[i];
    }

    /* A BARE ACKNOWLEDGE IS ONE BYTE. Measured on the real ND-324716 firmware
     * (RetroCore AccpNd500MonStartupHandshakeTests, 2026-09-18) and the shape its
     * station sends by default - OctobusND5000Station.cs:640,
     * RealFirmwareReplyShape = true, whose own note records that two full-ladder runs
     * with these shapes "loaded the control store and the swapper and ran a domain to
     * MON 0B". nd500x sent two bytes, 00 00, which is the shape that property turns
     * OFF. An acknowledge that returns data carries the data after the status byte. */
    int count = (param_count > 0) ? (1 + param_count) : 1;

    return ndbus_multibyte_build(station, reply_omd, (uint8_t)NDBUS_ACCP_OMD, payload, count,
                                 replies, max);
}

/*
 * Messnak.
 *
 * Ported from RetroCore OctobusND5000Station.cs SendAccpMessnak, including the
 * reason the first byte is 0xFF: the reply's discriminator is the status HIGH
 * byte of the first word, read the way 5OMBREAD reads CSTS - MFACK (0x00) is an
 * ack, MFNACK (0xFF = 377B) is a nak. A raw error code in that byte is neither,
 * and reads as "no answer". So MFNACK goes in the high byte and the error code
 * travels in the low one: [0xFF][error code][ASTS].
 *
 * The short form - measured as exactly FF 0D - is arm 0x0D's refusal, the one nak
 * the real firmware sends with no status byte.
 */
static int build_messnak(uint8_t station, uint8_t reply_omd, int nak_code, bool short_form,
                         uint16_t *replies, int max)
{
    uint8_t payload[4];
    int     count;

    payload[0] = 0xFF; /* MFNACK */
    payload[1] = (uint8_t)(nak_code & 0xFF);

    if (short_form)
    {
        count = 2;
    }
    else
    {
        /* FOUR BYTES, and the two status bytes are 0x10 0x11. Same measurement and
         * same default as the acknowledge above: the firmware naks with
         *     [0xFF MFNACK][error code][ASTS high 0x10][ASTS low 0x11]
         * and the status pair reads 10 11 while the microprogram is not running
         * (OctobusND5000Station.cs:4217, constants at :3678 and :3681). nd500x sent
         * three bytes with a zero ASTS. */
        payload[2] = NDBUS_ACCP_ASTS_HIGH;
        payload[3] = NDBUS_ACCP_ASTS_LOW;
        count      = 4;
    }

    return ndbus_multibyte_build(station, reply_omd, (uint8_t)NDBUS_ACCP_OMD, payload, count,
                                 replies, max);
}

/* ---- ND-500 address zero (ADRZERO) -------------------------------------------
 *
 * LPARP's pointer counts bytes from ND-500 physical address 0, which SINTRAN
 * calls ADRZERO. The pool counts bytes from the start of the shared window. The
 * two are the same number only while ADRZERO is the window's own ND-100 base, so
 * every use of the pointer goes through nd5000_resolve_nd500_byte().
 *
 * Ported from OctobusND5000Station.cs: field _nd500ZeroByte (line 252), property
 * Nd500ZeroByte (line 258), ResolveNd500Byte (line 269) and CalibrateNd500Zero
 * (lines 3885-3929). That file works in ND-100 physical byte addresses, with
 * _mpm.Start as the window's base and _mpm.Size as its size. Here the window IS
 * the pool: pool byte 0 is ND-100 byte servicer.nd100_window_base_byte (set by
 * the embedding, 0 when the pool is the ND-100's whole view) and the window size
 * is the pool size. So an ND-100 address becomes a pool offset by subtracting
 * that base, and nothing else is converted.
 */

/* The ND-100 physical byte address of pool byte 0: the reference's _mpm.Start. */
static uint32_t nd5000_window_base(const NdbusNd5000 *nd)
{
    return nd->servicer.nd100_window_base_byte;
}

/* ND-100 physical byte address of ND-500 physical address 0, or the window base
 * while it is still uncalibrated. C# Nd500ZeroByte, line 258. */
static uint32_t nd5000_nd500_zero_byte(const NdbusNd5000 *nd)
{
    return (nd->nd500_zero_byte != 0u) ? nd->nd500_zero_byte : nd5000_window_base(nd);
}

/* An ND-500-relative byte offset as a POOL byte offset. C# ResolveNd500Byte, line
 * 269, gives the ND-100 address Nd500ZeroByte + offset; the window base comes off
 * to index the pool. Unsigned 32-bit arithmetic throughout, as in the reference,
 * so an address below the window turns into an offset far past the pool's end,
 * where a read returns 0 and a write is refused - the same result MpmWindow gives
 * for an address outside the window. */
static uint32_t nd5000_resolve_nd500_byte(const NdbusNd5000 *nd, uint32_t nd500_byte_offset)
{
    return (nd5000_nd500_zero_byte(nd) + nd500_byte_offset) - nd5000_window_base(nd);
}

/* One ND 32-bit double at a pool offset: two big-endian words, high word first,
 * EACH bounds-checked on its own. C# ReadMpm32 (line 3735) -> MpmWindow.ReadDouble
 * ($RETROCORE/Emulated.HW/ND/CPU/NDBUS/MpmWindow.cs line 131), which is two
 * ReadWord calls; ndbus_pool_read32 refuses the whole value when either half is
 * outside, which is a different answer for a double that straddles the end. */
static uint32_t nd5000_read_mpm32(const NdbusNd5000 *nd, uint32_t pool_offset)
{
    return ((uint32_t)ndbus_pool_read16(nd->pool, pool_offset) << 16u) |
           (uint32_t)ndbus_pool_read16(nd->pool, pool_offset + 2u);
}

/**
 * @brief Work out ADRZERO from the test word SINTRAN wrote, and remember it.
 *
 * Ported from OctobusND5000Station.cs CalibrateNd500Zero, lines 3885-3929.
 *
 * WHY THIS IS A MEASUREMENT AND NOT A GUESS, in that routine's own terms: VPARP is
 * the one exchange that carries a value known independently - SINTRAN writes
 * NDBUS_ACCP_VPARP_TEST_PATTERN at the pointer and compares the echo against it.
 * So finding that exact word in the window at address A means the parameter area
 * is at A, and ADRZERO is A minus the pointer. Two guards keep a coincidence from
 * being adopted: the derived base must be 2048-byte aligned, because ADRZERO is
 * an ND-100 PAGE number; and exactly one candidate must survive, because several
 * equally plausible hits prove nothing and are refused rather than picked between.
 *
 * @param nd      The station.
 * @param pointer The pointer LPARP delivered, an ND-500-relative byte offset.
 * @return true when a NEW base was established; false when nothing usable was
 *         found, when the candidates are ambiguous, or when the base found is the
 *         one already in use.
 */
static bool nd5000_calibrate_nd500_zero(NdbusNd5000 *nd, uint32_t pointer)
{
    if (nd->pool == NULL || nd->pool->bytes == NULL || nd->pool->size < 4u)
    {
        return false;
    }

    const uint16_t hi   = (uint16_t)(NDBUS_ACCP_VPARP_TEST_PATTERN >> 16u);
    const uint16_t lo   = (uint16_t)(NDBUS_ACCP_VPARP_TEST_PATTERN & 0xFFFFu);
    const uint32_t base = nd5000_window_base(nd);

    uint32_t found = 0u;
    int      hits  = 0;

    /* The reference walks a = _mpm.Start while a < _mpm.Start + _mpm.Size - 4, in
     * steps of 2. `offset` is that same walk with the window base taken off, and
     * `a` is put back together below because the two tests are on the ND-100
     * address. */
    const uint32_t end = nd->pool->size - 4u;
    for (uint32_t offset = 0u; offset < end; offset += 2u)
    {
        if (ndbus_pool_read16(nd->pool, offset) != hi ||
            ndbus_pool_read16(nd->pool, offset + 2u) != lo)
        {
            continue;
        }

        uint32_t a = base + offset;
        if (a < pointer)
        {
            continue; /* would put ADRZERO below zero */
        }
        uint32_t candidate = a - pointer;
        if ((candidate & 0x7FFu) != 0u)
        {
            continue; /* ADRZERO is a PAGE number: 2048-byte aligned */
        }
        if (hits == 0)
        {
            found = candidate;
            hits  = 1;
        }
        else if (candidate != found)
        {
            hits++;
        }
    }

    if (hits != 1)
    {
        if (hits == 0)
        {
            nd_log(nd, "ND-5000 ACCP: ADRZERO calibration: pattern 0x65969B49 not found at any "
                       "page-aligned offset - leaving the base where it is");
        }
        else
        {
            char text[160];
            (void)snprintf(text, sizeof(text),
                           "ND-5000 ACCP: ADRZERO calibration: %d different page-aligned "
                           "candidates - ambiguous, refusing to pick one",
                           hits);
            nd_log(nd, text);
        }
        return false;
    }

    if (found == nd5000_nd500_zero_byte(nd))
    {
        return false; /* already right; nothing to change */
    }

    char text[200];
    (void)snprintf(text, sizeof(text),
                   "ND-5000 ACCP: ADRZERO calibrated to ND-100 byte 0x%08X (ND-100 page %oB) "
                   "from the VPARP test pattern, not the window base 0x%08X",
                   (unsigned)found, (unsigned)(found / 2048u), (unsigned)base);
    nd_log(nd, text);
    nd->nd500_zero_byte = found;
    return true;
}

/**
 * @brief VERIFY PARAMETER POINTER: return the 32-bit word from the parameter area.
 *
 * THE ONE COMMAND A CANNED MESSACK CANNOT SATISFY.
 *
 * T128: "This command is used to verify that the ND-120 and the ACCP agree on
 * where the parameter area is. Before the command is given, the ND-120 writes a
 * 32-bit word in the parameter area. The ACCP reads and returns the word from
 * its parameter area, and the ND-120 should then check if they are equal."
 *
 * So the reply must come out of SHARED MEMORY at the pointer LPARP gave.
 * Answering with a fixed value passes the guard and fails the check the command
 * exists to perform - and the ND-120 concludes the two disagree about where the
 * parameter area is, which is a confusing thing to debug when the real answer is
 * "the emulator never looked".
 *
 * Most significant byte first (T124).
 *
 * @param nd      The station.
 * @param replies Reply frames to fill.
 * @param max     Room in replies.
 * @return Number of reply frames written: Messack plus four parameter bytes.
 */
static int accp_vparp(NdbusNd5000 *nd, uint8_t reply_omd, uint16_t *replies, int max)
{
    /* THE POINTER IS RELATIVE TO ADRZERO, NOT TO POOL BYTE 0. Ported from
     * $RETROCORE/Emulated.HW/ND/CPU/NDBUS/OctobusND5000Station.cs SendVparpEcho,
     * lines 3955-3961: read at the resolved address, and when the word there is
     * not the test pattern, try to calibrate ADRZERO from the pattern SINTRAN
     * itself wrote and read again at the corrected address - so the first VPARP
     * after a memory reconfiguration still passes instead of failing the whole
     * control-store load. */
    uint32_t addr = nd5000_resolve_nd500_byte(nd, nd->parameter_pointer);
    uint32_t word = nd5000_read_mpm32(nd, addr);
    if (word != NDBUS_ACCP_VPARP_TEST_PATTERN &&
        nd5000_calibrate_nd500_zero(nd, nd->parameter_pointer))
    {
        addr = nd5000_resolve_nd500_byte(nd, nd->parameter_pointer);
        word = nd5000_read_mpm32(nd, addr);
    }

    uint8_t  reply[4];
    reply[0] = (uint8_t)(word >> 24u);
    reply[1] = (uint8_t)(word >> 16u);
    reply[2] = (uint8_t)(word >> 8u);
    reply[3] = (uint8_t)(word & 0xFFu);
    return build_messack(nd->station.number, reply_omd, reply, 4, replies, max);
}

/* ---- the control-store load, and the checksum it has to satisfy -------------
 *
 * MEASURED ON THE LIVE MACHINE, 30-SEP-2026, with every OMD-3 command logged as
 * it arrived at this station. SINTRAN's ND-500/5000 monitor J04, on
 * START-SWAPPER, sends:
 *
 *     LPARP  pointer = 0x00000800
 *     VPARP  (the pattern 6596 9B49 is at the pointer, and is echoed)
 *     LOCSM  x 128, each with parameter word0 = 0x0080 (N = 128 microwords) and
 *            word1 = the control-store address: 0x0000, 0x0080, ... 0x3F80
 *     DUCS   x 1, with word0 = 0x0001 (N = 1) and word1 = 0x0000
 *
 * and then prints "Error when loading Control Store. / ACCP command status:
 * Checksum error". The 128 write pulses and the single read-back pulse used to
 * fall through to the "accepted and not modelled" tail of run_command, so NOTHING
 * moved: the monitor summed whatever was left lying in the parameter field and
 * compared it against a stale addend word. The measured failing pair was
 *
 *     the eight halfwords at the pointer   0001 0000 0040 0000 0001 8000 0000 0000
 *     their sum                            0x8042
 *     the addend word at pointer + 16      0x0080   (N from the last LOCSM pulse)
 *
 * so the compare could only ever fail. It is not a byte-order fault; it is an
 * unimplemented command.
 *
 * Ported from RetroCore
 * $RETROCORE/Emulated.HW/ND/CPU/NDBUS/OctobusND5000Station.cs:
 *   - ServiceControlStoreWrite (line 4007): parameter word0 = N, word1 = the
 *     control-store microword address, then N * 8 halfwords of microword data at
 *     pointer + 4, copied verbatim into the store.
 *   - ServiceControlStoreReadback (line 4119): N * 8 microword halfwords are
 *     served at the pointer, and the halfword straight after them is their 16-bit
 *     wrapping sum - the checksum addend. The same layout manual ND-05.020.01
 *     sec 5.3.20 states: "the dumped N x (8 x 16b) microwords + checksum addend".
 *
 * WHERE THIS DELIBERATELY DIFFERS FROM RetroCore, AND WHY. RetroCore does not
 * write the dump into the parameter field; it intercepts the ND-100's reads of
 * that field and answers them (TryOverrideMpmRead, line 387). Its own comment
 * gives the reason: in that emulator a concurrent reader interleaves with the
 * monitor's sum loop, which made a written block unreliable to observe. There is
 * no such interleaving here, and the real card DMAs the dump into the parameter
 * field - which is what sec 5.3.20 describes - so this writes the same bytes
 * RetroCore would have served, through the existing pool helpers, and needs no
 * read hook in the ND-100. The bytes the monitor sums are identical either way.
 *
 * NO MICROCODE IS EXECUTED from this buffer. It is the load's memory, nothing
 * more.
 */

/* Make sure the control store exists. Allocated on the first LOCSM pulse rather
 * than at init because it is 256 KB per station and a machine with no ND-5000
 * microcode load never needs it. */
static bool cs_ensure(NdbusNd5000 *nd)
{
    if (nd->control_store != NULL)
    {
        return true;
    }
    nd->control_store = (uint16_t *)calloc((size_t)NDBUS_CS_HALFWORDS, sizeof(uint16_t));
    if (nd->control_store == NULL)
    {
        nd_log(nd, "ND-5000 ACCP: no memory for the control store - CS load ignored");
        return false;
    }
    return true;
}

/*
 * One 16-bit slice of a 128-bit microword. Slice 0 is bits 127-112 and slice 7 is
 * bits 15-0, which is the order the ACCP shifts them and the order the write
 * pulse stored them - RetroCore ReadCsHalfword, OctobusND5000Station.cs line 425.
 * Out of range reads as zero, the same as an unwritten slot.
 */
static uint16_t cs_read_halfword(const NdbusNd5000 *nd, uint32_t cs_word, uint32_t halfword)
{
    if (nd->control_store == NULL || halfword >= NDBUS_CS_HALFWORDS_PER_WORD ||
        cs_word >= NDBUS_CS_WORD_COUNT)
    {
        return 0u;
    }
    return nd->control_store[(cs_word * NDBUS_CS_HALFWORDS_PER_WORD) + halfword];
}

/*
 * The microprogram model/version report ENKICK sends on top of its acknowledge.
 *
 * Ported from RetroCore OctobusND5000Station.cs SendMicroprogramModelReport
 * (lines 3631-3670). Six bytes, as a TRAP_OCBM multibyte message:
 *     [0x82][0x01][cpuModel][cpuModel][version hi][version lo]
 * 0x82 is FaultType 202B, NotFatal - "CPU available"; 0x01 is ErrorReporter 1,
 * MicroProgram.
 *
 * THE TWO VALUES COME OUT OF THE LOADED CONTROL STORE, never from a constant, so a
 * wrong image cannot silently produce a right-looking model. Word N of the image is
 * microword N and both fields sit in the LAST halfword of their word:
 *     version  = word 1, halfword 7  (LARG)     - 0x2E9A on the 5800-B30 image
 *     cpuModel = word 7, halfword 7, low byte   - 0x38 on the 5800-B30 image
 * The two CPUMODEL bytes are "Current" (the ACCP/backplane side) and "My" (the
 * loaded store), equal by construction here because both are read from the store.
 *
 * WHY IT MATTERS, in RetroCore's own words at that call site: the ND-100 monitor is
 * in a busy-wait that only an inbound octobus frame breaks, and the report carries
 * the control-store-derived model byte that gates "Wrong microprogram" (EWRON).
 * nd500x sent only the bare acknowledge.
 *
 * The destination OMD is the runtime-allocated 5OMDNO out of LSYSPAR S5, never a
 * constant, and the SOURCE OMD is 4 - not the 3 an ordinary ACCP reply uses.
 *
 * THE DESTINATION STATION IS WHOEVER SENT ENKICK. The reference ends with
 *     SendMultibyteMessage(Fabric, _accpMessage.Source, destOmd, sourceOmd: 4, report)
 * (line 3669), and _accpMessage.Source is the station the ENKICK message came
 * from. Until 05-OCT-2026 this routine built the frames with the station's OWN
 * number in the station field and handed them to ndbus_fabric_send(), which reads
 * that field as the destination - so the report was delivered straight back to
 * this station, which dropped it, and the ND-100 never saw it.
 *
 * So the frames are now written into the same reply buffer as the ENKICK
 * acknowledge, directly after it. The fabric hands that buffer to the station
 * whose frame is being answered, which is the ENKICK sender, and the frames reach
 * it in buffer order - acknowledge first, report second, as lines 3276 and 3286
 * send them. Like every reply frame they carry this station's number in bits
 * 13-8, which is what the receiver would see after the fabric's rewrite.
 *
 * @param nd      The station.
 * @param replies Where to write the report's frames - the first free slot after
 *                the acknowledge.
 * @param max     Room left in replies, in frames.
 * @return The number of frames written: 10 for the six-byte report, or 0 when
 *         there is no control store to read the two values from (counted in
 *         report_no_store) or the frames do not fit.
 */
static int nd5000_build_microprogram_model_report(NdbusNd5000 *nd, uint16_t *replies, int max)
{
    if (nd->control_store == NULL)
    {
        nd->report_no_store++;
        return 0;
    }

    uint16_t version   = cs_read_halfword(nd, 1u, 7u);
    uint8_t  cpu_model = (uint8_t)(cs_read_halfword(nd, 7u, 7u) & 0xFFu);

    const uint8_t report[6] = {
        0x82u, 0x01u, cpu_model, cpu_model,
        (uint8_t)(version >> 8), (uint8_t)(version & 0xFFu),
    };

    uint8_t dest_omd = (uint8_t)((nd->lsyspar_word1 >> 8) & 0x3Fu);
    int     n = ndbus_multibyte_build(nd->station.number, dest_omd, 4u, report,
                                      (int)sizeof(report), replies, max);
    if (n <= 0)
    {
        return 0;
    }
    nd->model_reports++;
    nd->last_model_report_model = cpu_model;
    nd->last_model_report_version = version;
    return n;
}


/**
 * @brief LOCSM 023B: DMA one control-store page out of the parameter area.
 *
 * The parameter area, at ADRZERO plus the pointer LPARP gave, holds word0 = N,
 * the number of 128-bit microwords in this pulse; word1 = the control-store
 * microword address to load them at; then N * 8 halfwords of microword data.
 *
 * @param nd The station. Its parameter pointer is already known to be set - the
 *           guard table in ndbus_accp.c refuses LOCSM without one.
 * @return Nothing. A pulse with N = 0, or one that runs off the end of the
 *         control store, copies what fits and stops.
 */
static void accp_load_control_store(NdbusNd5000 *nd)
{
    /* The parameter block is at ADRZERO + pointer, not at pool offset `pointer`:
     * OctobusND5000Station.cs ServiceControlStoreWrite, line 4024,
     * "uint pb = ResolveNd500Byte(_accpParameterPointer)". */
    uint32_t pb       = nd5000_resolve_nd500_byte(nd, nd->parameter_pointer);
    uint32_t count    = ndbus_pool_read16(nd->pool, pb);
    uint32_t cs_word  = ndbus_pool_read16(nd->pool, pb + 2u);

    if (count == 0u || !cs_ensure(nd))
    {
        return;
    }

    uint32_t dst       = cs_word * NDBUS_CS_HALFWORDS_PER_WORD;
    uint32_t halfwords = count * NDBUS_CS_HALFWORDS_PER_WORD;
    for (uint32_t i = 0u; i < halfwords; i++)
    {
        if ((dst + i) >= NDBUS_CS_HALFWORDS)
        {
            break;
        }
        /* The data region starts one 32-bit parameter word in, at pb + 4, and the
         * halfwords are consecutive from there. */
        nd->control_store[dst + i] = ndbus_pool_read16(nd->pool, pb + 4u + (i * 2u));
    }

    nd->cs_load_pulses++;
}

/**
 * @brief DUCS 025B: DMA one control-store page back into the parameter area.
 *
 * Parameter word0 = N, word1 = the control-store microword address, exactly as
 * LOCSM. The dump is written AT the parameter pointer - N * 8 microword
 * halfwords - and the halfword straight after them is their 16-bit wrapping sum,
 * which is the checksum addend the ND-100 compares its own sum against.
 *
 * The sum is taken from the control store, NOT from the bytes already in the
 * parameter field, because the field is stale by now: the last LOCSM pulse left
 * its own header there. RetroCore makes the same point at
 * OctobusND5000Station.cs:4136 ("THE CHECKSUM MUST COME FROM THE SAME STORE THE
 * READ-BACK WILL SERVE").
 *
 * @param nd The station. Its parameter pointer is already known to be set.
 * @return Nothing. A pulse with N = 0 writes nothing.
 */
static void accp_dump_control_store(NdbusNd5000 *nd)
{
    /* The same resolve as LOCSM: OctobusND5000Station.cs
     * ServiceControlStoreReadback, line 4139. ONLY THE ADDRESS IS PORTED HERE. The
     * reference then leaves memory alone and answers the ND-100's READS of this
     * block (TryOverrideMpmRead, lines 456-491); this routine still WRITES the dump
     * into the pool, which the block comment above ("WHERE THIS DELIBERATELY
     * DIFFERS FROM RetroCore") states as a decision. That difference is unchanged. */
    uint32_t pb      = nd5000_resolve_nd500_byte(nd, nd->parameter_pointer);
    uint32_t count   = ndbus_pool_read16(nd->pool, pb);
    uint32_t cs_word = ndbus_pool_read16(nd->pool, pb + 2u);

    if (count == 0u)
    {
        return;
    }

    uint32_t halfwords = count * NDBUS_CS_HALFWORDS_PER_WORD;
    uint32_t sum       = 0u;
    for (uint32_t i = 0u; i < halfwords; i++)
    {
        uint16_t slice = cs_read_halfword(nd, cs_word + (i / NDBUS_CS_HALFWORDS_PER_WORD),
                                          i % NDBUS_CS_HALFWORDS_PER_WORD);
        sum            = (sum + slice) & 0xFFFFu;
        (void)ndbus_pool_write16(nd->pool, pb + (i * 2u), slice);
    }
    (void)ndbus_pool_write16(nd->pool, pb + (halfwords * 2u), (uint16_t)sum);

    nd->cs_dump_pulses++;
    nd->cs_last_dump_words = halfwords;
    nd->cs_last_addend     = (uint16_t)sum;
}

/* ---- the cells SINTRAN patches into the control store ------------------------ */

/** Read a control-store cell's LARG field: the 32-bit value in halfwords 6 and 7,
 *  high half first. C# ReadControlStoreLarg, OctobusND5000Station.cs line 1559.
 *
 *  READ THE FULL 32 BITS, NEVER JUST HALFWORD 7. RetroCore records the cost of the
 *  short read: the context-block cell holds 0x0002A000 on a real machine, so a
 *  halfword-7 read returns 0xA000 - a plausible-looking wrong value - and feeding
 *  that on made the 23B start run with a garbage context and the run died. */
static uint32_t cs_read_larg(const NdbusNd5000 *nd, uint32_t cs_word)
{
    if (nd->control_store == NULL)
    {
        return 0u;
    }

    return ((uint32_t)cs_read_halfword(nd, cs_word, 6u) << 16u) |
           (uint32_t)cs_read_halfword(nd, cs_word, 7u);
}

/**
 * The per-model CPU parameter 3RMICV reports, selected by the control store's
 * model byte.
 *
 * Ported from RetroCore OctobusND5000Station.cs TryCpuParForModelByte. Only model 8
 * is verified end to end, and an unrecognised byte returns 0 rather than a guess:
 * ndbus_servicer_set_cpu_identity() then keeps what it has, so a different image is
 * visibly unmapped instead of silently relabelled as a 5800.
 *
 * @param model_byte The packed model byte from control-store word 7, halfword 7.
 * @return The CPUPAR value, or 0 when there is no verified mapping.
 */
static uint16_t cpupar_for_model_byte(uint8_t model_byte)
{
    switch (model_byte)
    {
    case 0x38u:
        /* 5800, model 8: microcode CPUMOD08 @017243 SARG = 001741B. */
        return 0x03E1u;

    default:
        return 0u;
    }
}

/**
 * Load the memory-management pointers SINTRAN patched into the control store, at
 * microprogram start.
 *
 * Ported from OctobusND5000Station.cs LoadMmsPointersFromControlStore, lines
 * 1568-1644, which the 066B/035B/036B arm calls at line 3396.
 *
 * THESE ARE NOT VALUES ANY MESSAGE CARRIES. SINTRAN writes them into control-store
 * cells before micro-start and the microcode reads them from there, so the only
 * honest source is the store this station loaded:
 *
 *   cell 0o21  PSTBASE  the physical segment table, as an ND-500 PAGE. Shifted by
 *                       11 to bytes - ND-05.020.01 section 6.6.
 *   cell 0o20  OFFSET   the start of the context (register) block area, ALREADY a
 *                       byte address, so it is NOT shifted. ND-05.017.01 App. A.1
 *                       names it, and GET_CNTXT @0o13372 and CNTXTLOAD @0o14746
 *                       both jump to the cell.
 *
 * THE TWO CELLS ARE INDEPENDENT, and neither depends on START_MESS. The reference
 * says why in its own comment: it used to return when PSTBASE read zero, which
 * skipped the OFFSET read entirely, and a store with 0o21 unpatched but 0o20
 * patched then left the context block area at zero "with nothing saying why".
 * Until 05-OCT-2026 this port read both cells only from ENKICK and only after a
 * mailbox had been attached, so a store with START_MESS still zero - or a start
 * with no ENKICK after it - left both unread.
 *
 * The reference also writes PSTP into its CPU here (line 1620). This station has
 * no CPU, so the value is left in `pst_base` for the embedding to take.
 */
static void nd5000_load_mms_pointers_from_control_store(NdbusNd5000 *nd)
{
    if (nd->control_store == NULL)
    {
        return;
    }

    uint32_t pst_page = cs_read_larg(nd, 0x11u); /* 0o21 PSTBASE */
    if (pst_page != 0u)
    {
        nd->pst_base = pst_page << 11u;
        (void)ndbus_servicer_set_pst_base(&nd->servicer, nd->pst_base);
        nd_log(nd, "ND-5000 ACCP: physical segment table located from patched CS cell 0o21");
    }
    else
    {
        /* Said out loud rather than left as a zero: with no table the segment
         * relative copy members fall back to a flat address, which is the defect
         * that put a process control block 0xD9000 bytes from where the machine
         * looks for it. */
        nd_log(nd, "ND-5000 ACCP: CS cell 0o21 PSTBASE is zero - the store is not patched, so "
                   "PHYSRD/PHYSWR will use flat addresses");
    }

    uint32_t context_area = cs_read_larg(nd, 0x10u); /* 0o20 OFFSET, already bytes */
    if (context_area != 0u)
    {
        nd->context_area = context_area;
        (void)ndbus_servicer_set_context_area(&nd->servicer, context_area);
        nd_log(nd, "ND-5000 ACCP: context block area located from patched CS cell 0o20");
    }
    else
    {
        nd_log(nd, "ND-5000 ACCP: CS cell 0o20 OFFSET is zero - the store is not patched, so no "
                   "process start can be taken");
    }
}

/**
 * Take the 3RMICV reply fields from the loaded control store, at microprogram start.
 *
 * Ported from OctobusND5000Station.cs DeriveCpuIdentityFromControlStore, lines
 * 1662-1705, which the 066B/035B/036B arm calls at line 3402:
 *
 *   word 1     LARG halfword 7: the microprogram version 3RMICV answers.
 *   word 7     halfword 7 low byte: the model byte that selects CPUPAR.
 *
 * A zero version (store not loaded or not patched) and a model byte with no
 * verified CPUPAR both keep the value the servicer already has; that rule is
 * inside ndbus_servicer_set_cpu_identity().
 */
static void nd5000_derive_cpu_identity_from_control_store(NdbusNd5000 *nd)
{
    if (nd->control_store == NULL)
    {
        return;
    }

    uint16_t version    = cs_read_halfword(nd, 1u, 7u);
    uint8_t  model_byte = (uint8_t)(cs_read_halfword(nd, 7u, 7u) & 0xFFu);
    (void)ndbus_servicer_set_cpu_identity(&nd->servicer, version,
                                         cpupar_for_model_byte(model_byte));
}

/*
 * The command itself. Only the state the guards read is maintained here; the
 * commands that move real data (the control store, the multiport test) are not
 * implemented and say so rather than pretending to have worked.
 */
/*
 * Run one received ACCP message.
 *
 * THE MESSAGE HAS A TWO-BYTE HEADER IN FRONT OF THE COMMAND, and reading past it
 * is not optional:
 *
 *     message = [ source OMD ] [ byte count ] [ COMMAND ] [ parameters... ]
 *
 * so the command is byte 2 and the parameter count is length - 3. Ported from
 * RetroCore OctobusND5000Station.cs ExecuteAccpMultibyteCommand, which says the
 * same thing twice - `byte command = message[2]` and "message = [srcOMD, count,
 * command, parameters...], so the parameter count is Length - 3".
 *
 * This was read as byte 0 here until 26-SEP-2026, which made every command two
 * bytes too early. Captured from SINTRAN's own ND-500 monitor, the message to
 * station 070B is
 *
 *     SOMB omd=3 | 03 07 0E 01 03 00 00 00 00 | EOMB omd=3
 *
 * i.e. source OMD 3, count 7, command 016B. Reading byte 0 saw command 003B,
 * which is below the armed range, so the station refused a command it implements
 * and the monitor reported "No ND-500(0) CPU found".
 *
 * The reply goes to the SOURCE OMD out of byte 0 - the OMD the sender listens on -
 * never to a constant.
 */
static int run_command(NdbusNd5000 *nd, const uint8_t *body, int length, uint16_t *replies,
                       int max)
{
    /* Source OMD, byte count and a command byte: below that there is no command
     * to run, and no OMD to answer on either. */
    if (length < 3)
    {
        nd_log(nd, "ND-5000 ACCP: message shorter than its own header - no reply");
        return 0;
    }

    uint8_t reply_omd = (uint8_t)(body[0] & 0x0Fu);
    uint8_t command   = body[2];
    nd->last_command  = command;

    /* The parameters, and how many of them - the command's own bytes, with the
     * header and the command byte behind them. */
    const uint8_t *params      = body + 3;
    int            param_count = length - 3;

    /*
     * LENGTH IS CHECKED BEFORE THE GUARDS for LOCSD and DCSD only, and a message
     * that is too short gets NO REPLY AT ALL - not a Messnak. A caller that
     * treats silence as an error reports a fault the real card does not report.
     */
    int needed = ndbus_accp_min_parameter_bytes(command);
    if (ndbus_accp_length_checked_first(command) && param_count < needed)
    {
        nd_log(nd, "ND-5000 ACCP: short LOCSD/DCSD - no reply, exactly as the card");
        return 0;
    }

    int verdict = ndbus_accp_evaluate(command, &nd->accp);
    if (verdict != NDBUS_ACCP_ACCEPTED)
    {
        nd->messnaks++;
        nd->last_nak_code = verdict;
        return build_messnak(nd->station.number, reply_omd, verdict,
                             ndbus_accp_nak_is_short(verdict), replies, max);
    }

    /* Every other command needs its parameters too; below the measured length
     * the card also stays silent. */
    if (param_count < needed)
    {
        nd_log(nd, "ND-5000 ACCP: short command - no reply, exactly as the card");
        return 0;
    }

    switch (command)
    {
    case NDBUS_ACCP_LSYSPAR:
        /* THE THREE WORDS START AT THE IDENT BYTE. Verified against the current
         * RetroCore source, OctobusND5000Station.cs:3197: the CMSYSPAR handler sets
         * paramByteIndex = 3 and packs three 16-bit words from there, so
         *     word 1 = params[0] << 8 | params[1]
         * with params[0] = N100IDENT and params[1] = the 5OMDNO byte. For 5OMDNO 10B
         * that is 0x0108, NOT 0x0800.
         *
         * RETROCORE NAMES THE WRONG PARSE EXPLICITLY, at its GIVEINT site: "being
         * parsed one byte late (0x0800 instead of 0x0108, see the CMSYSPAR handler):
         * with the ident byte in place the plain expression yields 100401B = 0x8101 =
         * station 1, the value observed on the live machine."
         *
         * nd500x was wrong here twice. It started TWO bytes late (params[2..3]), which
         * captured 0x0000 and addressed every answer interrupt to station 0; commit
         * b311a1a then moved it to ONE byte late (params[1..2]) and added a >> 3 to the
         * frame arithmetic to compensate - the exact parse that comment warns against.
         * Word 1 holds the IDENT byte in bits 15-8, and that is what the frame's 0x3F00
         * mask takes as the destination station.
         *
         * The acknowledge goes on the SOURCE OMD, message[0]; the current tree agrees
         * (OctobusND5000Station.cs:3235) and an earlier note here claiming it answers
         * on the S5 OMD described a stale copy. */
        for (uint32_t w = 0; w < 3u; w++)
        {
            uint32_t hi = 0u + w * 2u;
            nd->system_parameters[w] =
                (uint16_t)(((uint16_t)params[hi] << 8u) | params[hi + 1u]);
        }
        nd->lsyspar_word1 = nd->system_parameters[0];
        nd->accp.system_parameters_given = true;
        break;

    /* ---- the commands that RETURN DATA ---------------------------------------
     *
     * Bytes as measured on the real ND-324716 firmware, state matrix 2026-09-18, and
     * sent by RetroCore's station by default (OctobusND5000Station.cs:3289-3347).
     * nd500x answered all five with the bare acknowledge. The status byte is
     * prepended by build_messack, so each arm supplies only what follows it. */

    case NDBUS_ACCP_RASTS:
        /* 050B: the 16-bit ACCP status word, which reads 10 11. */
        {
            const uint8_t asts[2] = { NDBUS_ACCP_ASTS_HIGH, NDBUS_ACCP_ASTS_LOW };
            nd->messacks++;
            nd->last_nak_code = NDBUS_ACCP_ACCEPTED;
            return build_messack(nd->station.number, reply_omd, asts, 2, replies, max);
        }

    case NDBUS_ACCP_RSSYSPAR:
        /* 015B: the three system-parameter words CMSYSPAR stored, read back. The
         * guard above has already naked this with 13 if none were ever given. */
        {
            uint8_t back[6];
            for (uint32_t w = 0; w < 3u; w++)
            {
                back[w * 2u]      = (uint8_t)(nd->system_parameters[w] >> 8);
                back[w * 2u + 1u] = (uint8_t)(nd->system_parameters[w] & 0xFFu);
            }
            nd->messacks++;
            nd->last_nak_code = NDBUS_ACCP_ACCEPTED;
            return build_messack(nd->station.number, reply_omd, back, 6, replies, max);
        }

    case NDBUS_ACCP_PRGMVERS:
        /* 075B: twelve ASCII bytes, "88.12. 5 I01" - the PROM version string the real
         * firmware holds at ROM offset 0x13BF4. This is what the ND-500/5000 monitor's
         * VERSION command prints as "Accp version". */
        {
            static const uint8_t version[12] = {
                0x38u, 0x38u, 0x2Eu, 0x31u, 0x32u, 0x2Eu,
                0x20u, 0x35u, 0x20u, 0x49u, 0x30u, 0x31u,
            };
            nd->messacks++;
            nd->last_nak_code = NDBUS_ACCP_ACCEPTED;
            return build_messack(nd->station.number, reply_omd, version, 12, replies, max);
        }

    case NDBUS_ACCP_RECO:
        /* 020B: sixteen words from firmware RAM, all zero on a card that has loaded
         * nothing. Thirty-two zero bytes after the status byte. */
        {
            uint8_t zeros[32];
            memset(zeros, 0, sizeof zeros);
            nd->messacks++;
            nd->last_nak_code = NDBUS_ACCP_ACCEPTED;
            return build_messack(nd->station.number, reply_omd, zeros, 32, replies, max);
        }

    case NDBUS_ACCP_READSELFT:
        /* 060B: the 16-bit self-test status. The value for a machine that PASSED is
         * not measured - the bare firmware reports 0x877F with no CPU cards behind it -
         * so this sends 0x0000, the value the real firmware holds after CPURES clears
         * it. RetroCore says the same in its own comment and sends the same. */
        {
            const uint8_t status[2] = { 0x00u, 0x00u };
            nd->messacks++;
            nd->last_nak_code = NDBUS_ACCP_ACCEPTED;
            return build_messack(nd->station.number, reply_omd, status, 2, replies, max);
        }

    case NDBUS_ACCP_VPARP:
        nd->messacks++;
        nd->last_nak_code = NDBUS_ACCP_ACCEPTED;
        return accp_vparp(nd, reply_omd, replies, max);

    /* ECHO 017B HAS NO ARM, AND THAT IS THE PORT. The reference's dispatcher has no
     * case for 0x0F, so it falls to the canned acknowledge at
     * OctobusND5000Station.cs line 3493: one status byte, no echoed data. Until
     * 05-OCT-2026 this port answered the acknowledge PLUS the count's worth of
     * echoed bytes, on the strength of manual T126 "Returns the test pattern".
     *
     * What is measured: $ND5000UC/docs/REAL-ACCP-COMMAND-STATE-MATRIX-2026-09-18.md,
     * parameter length sweep, "cmd 0x0F -> first reply at 1 parameter byte(s):
     * reply [ 00 ]". That message carried a count byte of ZERO, so it shows a bare
     * 00 and does not say what the real firmware returns for a non-zero count.
     * Both the old arm and the canned acknowledge give 00 for it. The echo the TPE
     * OCTOBUS B00 tests 5 and 6 perform is the OMD-0 Test Protocol's, in
     * ndbus_testproto.c, which is a different command on a different OMD and is
     * not touched by this. */

    case NDBUS_ACCP_LPARP:
        /* T128: "The address of the parameter area in the MFbus memory is
         * given." Four bytes, most significant first (T124). */
        nd->parameter_pointer = ((uint32_t)params[0] << 24u) | ((uint32_t)params[1] << 16u) |
                                ((uint32_t)params[2] << 8u) | (uint32_t)params[3];
        nd->accp.parameter_pointer_given = true;
        break;

    case NDBUS_ACCP_LOCSM:
        accp_load_control_store(nd);
        break;

    case NDBUS_ACCP_DUCS:
        accp_dump_control_store(nd);
        break;

    case NDBUS_ACCP_STARTMIC:
    /* CONTINUE AND RESTART START THE MICROPROGRAM TOO. RetroCore handles 066B
     * STARTMIC, 035B CONTMIC and 036B RESTMIC in ONE arm, all three setting the
     * running flag and clearing the idle one (OctobusND5000Station.cs:2857-2957,
     * where 0x1D and 0x1E share the 0x36 body). nd500x armed only STARTMIC, so a
     * CONTMIC fell through to the bare acknowledge and left the flag false.
     *
     * That flag is not bookkeeping. ENKICK's model/version report is sent only when
     * it is set, STOPMIC's refusal depends on it, and the whole guard matrix in
     * ndbus_accp.c keys off it - so a missed transition makes every later command
     * judged against a state the guest does not share.
     *
     * The GUARD MATRIX IS NOT CHANGED HERE. Ours comes from a newer RetroCore than
     * the tree these line numbers point at - our header cites AccpCommandGuards.cs,
     * which that tree does not contain - and the older tree acknowledges
     * unconditionally where ours refuses. Matching the older behaviour would be a
     * regression, so only the state transition is ported. */
    case NDBUS_ACCP_CONTMIC:
    case NDBUS_ACCP_RESTMIC:
        nd->accp.microprogram_running = true;
        /* STARTING THE MICROPROGRAM ALSO LEAVES THE IDLE STATE - lines 3366-3392 of
         * the reference. With the idle gate in nd5000_handle_frame() this is no
         * longer bookkeeping: SINTRAN sends 244B early in a normal bring-up, and a
         * flag that survived the start would drop every kick for the session. */
        nd->accp_idle = false;
        /* THE PATCHED CELLS ARE READ HERE, AT THE START, by all three commands -
         * lines 3396 and 3402. Each routine reads its own cells and neither looks
         * at START_MESS or at whether a mailbox exists. */
        nd5000_load_mms_pointers_from_control_store(nd);
        nd5000_derive_cpu_identity_from_control_store(nd);
        /* 066B ONLY: leave the functional CPU in its post-INIT state. Lines
         * 3447-3448, "if (command == 0x36 && _microcodeAdapter == null && _cpu is
         * CpuND500 funcCpu) funcCpu.ApplyND5000InitState();". CONTMIC and RESTMIC
         * resume where the microprogram stopped, so they do not run INIT again. */
        if (command == NDBUS_ACCP_STARTMIC && nd->apply_init_state != NULL)
        {
            nd->apply_init_state(nd->cpu_hook_ctx);
        }
        break;

    case NDBUS_ACCP_STOPMIC:
        nd->accp.microprogram_running = false;
        break;

    case NDBUS_ACCP_ENKICK:
        /* THE ORDER IS THE REFERENCE'S, lines 3275-3286: enable kicks, send the
         * acknowledge, take the mailbox base from the control store, and only then
         * - when the microprogram is running - the model/version report. */
        {
            nd->accp.kicks_enabled = true;

            nd->messacks++;
            nd->last_nak_code = NDBUS_ACCP_ACCEPTED;
            int count = build_messack(nd->station.number, reply_omd, NULL, 0, replies, max);

            /* ENKICK is the ACCP-to-microprogram handoff, and it is where the
             * mailbox has to be known: the control store is loaded by now, so the
             * patched START_MESS and SAMSON_CPU cells can be read instead of
             * guessed. A false return is not an error here - a machine whose
             * microcode was never patched simply has no mailbox to attach. */
            (void)ndbus_nd5000_mailbox_from_control_store(nd);

            /* AND THE MODEL/VERSION REPORT AFTER THE ACKNOWLEDGE, to the station
             * that sent ENKICK; see the routine's own comment for why the bare ack
             * is not enough and for how the destination is reached. */
            if (nd->accp.microprogram_running)
            {
                count += nd5000_build_microprogram_model_report(nd, replies + count, max - count);
            }
            return count;
        }

    case NDBUS_ACCP_DISKICK:
        nd->accp.kicks_enabled = false;
        break;

    case NDBUS_ACCP_CPURES:
        /* 071B RESETS THE ND-5000 CPU, NOT THE ACCP. Ported from lines 3151-3168:
         *     ResetCpuToIdle();
         *     _microprogramRunning = false;   // a CPU reset stops the microprogram
         *     DisableKicks();                 // measured: cell 0x1143B6, 0001 -> 0000
         * and nothing else. The system parameters, the parameter pointer and its
         * "given" cell all SURVIVE. Until 05-OCT-2026 this arm called
         * ndbus_nd5000_reset(), which also forgot the system parameters and the
         * parameter pointer and re-armed the doorbell sniff - state the reference
         * and the measured firmware leave alone. */
        if (nd->reset_cpu_to_idle != NULL)
        {
            nd->reset_cpu_to_idle(nd->cpu_hook_ctx);
        }
        nd->accp.microprogram_running = false;
        nd->accp.kicks_enabled        = false;
        break;

    case NDBUS_ACCP_ALIVE:
        /*
         * 037B: Messack while the microprogram is running, Messnak 7 ("not
         * alive") while it is not.
         *
         * THIS IS DECIDED HERE AND NOT IN THE GUARD TABLE. The real card answers
         * from a hardware alive signal rather than from the running cell, so
         * ndbus_accp_evaluate deliberately accepts ALIVE and says so at
         * ndbus_accp.c:122. RetroCore draws the same line - its
         * AccpCommandGuards.cs:75 states "ALIVE 0x1F is not decided here at
         * all", and its station answers the command in the dispatcher
         * (OctobusND5000Station.cs:3257) from _microprogramRunning, which is
         * the closest observable we have to that signal.
         *
         * The flip-flop is already driven correctly by the arms around this one:
         * STARTMIC, CONTMIC and RESTMIC set it; STOPMIC, CPURES, emergency 241B
         * master clear and emergency 244B TERMINATE ACCP clear it.
         *
         * An earlier attempt at this arm was reverted because it made the
         * monitor print "ND-5000 timeout: ACCP was terminated" at entry. That
         * was not this rule being wrong: the octobus card was dropping the tail
         * of any reply longer than its 16-word receive FIFO, the monitor
         * answered the mutilated reply with an emergency 244B TERMINATE ACCP,
         * and the terminate had cleared the flip-flop before ALIVE was asked.
         * With the frames no longer dropped the monitor does not terminate, and
         * on the measured bring-up it does not reach ALIVE at all.
         */
        if (!nd->accp.microprogram_running)
        {
            nd->messnaks++;
            nd->last_nak_code = NDBUS_ACCP_NAK_NOT_ALIVE;
            return build_messnak(nd->station.number, reply_omd, NDBUS_ACCP_NAK_NOT_ALIVE,
                                 ndbus_accp_nak_is_short(NDBUS_ACCP_NAK_NOT_ALIVE), replies, max);
        }
        break;

    default:
        /* Accepted by the guards and not modelled further. The guard table is
         * what SINTRAN's bring-up sequence actually tests; a command that moves
         * data gets its own arm when something needs it. */
        break;
    }

    nd->messacks++;
    nd->last_nak_code = NDBUS_ACCP_ACCEPTED;
    return build_messack(nd->station.number, reply_omd, NULL, 0, replies, max);
}

/* ---- kicks -------------------------------------------------------------------
 *
 * A kick is a control frame with K set and M clear, and its number is bits 5-0.
 * The microcode's OCB_DECODE -> OCB_MES_K computes VECT := word & 0o77 and jumps
 * into OCB_DEC_K (016430), a 64-entry table, which the reference states at
 * OctobusND5000Station.cs lines 2458-2463:
 *     0 -> NOTREC;  1,2 -> ACTIVATE;  3 -> OCB_KICK03;  4,5 -> OCB_KICK05;
 *     6 -> OCB_KICK06;  7-63 -> OCB_KICK64 (UNLOCK_QUE + NOTREC 204).
 * The three routines below are the arms the reference implements.
 *
 * Bit 5 is also the M flag, and the reference decodes a kick only when M is clear
 * (line 2399, "isControl && isKick && !isMultibyte"). So a frame carrying kick
 * number 32-63 is taken as multibyte traffic and never reaches this decoder; that
 * is the reference's behaviour and it is kept.
 */

/**
 * Kick 1 and 2: walk the ex-queue chain from X5BEX WITHOUT touching X5ACT.
 *
 * Ported from OctobusND5000Station.cs WalkQueue, lines 2072-2128 - the
 * microcode's ACTIVATE1 -> MSG_NEXTL path. "Kick 1 (N100KICK) enters here DIRECTLY
 * without touching X5ACT, like OCB_MES_K -> ACTIVATE": the kick is the PREEMPT
 * doorbell, and the idle wakeup is the X5ACT poll in
 * ndbus_nd5000_service_mailbox(), which is a separate path and is not called from
 * here.
 *
 * X5BEX is 32 bits at extension-block words 0-1, high half first, and holds a
 * WINDOW-RELATIVE BYTE offset. -1 is an empty chain; 0 is uninitialised and is
 * refused as an emulator guard (line 2117).
 *
 * @param nd The station, with a mailbox already located.
 * @return true when at least one message was answered by the walk.
 */
static bool nd5000_walk_queue(NdbusNd5000 *nd)
{
    uint32_t ext    = ndbus_mailbox_ext_base(&nd->mailbox);
    uint32_t bex_hi = ndbus_pool_read16(nd->pool, ext + NDBUS_MBX_X5BEX_WORD * 2u);
    uint32_t bex_lo = ndbus_pool_read16(nd->pool, ext + (NDBUS_MBX_X5BEX_WORD + 1u) * 2u);
    uint32_t bex    = (bex_hi << 16) | bex_lo;

    if (bex == 0xFFFFFFFFu || bex == 0u)
    {
        return false;
    }

    return ndbus_servicer_process_chain(&nd->servicer, bex & 0xFFFFFFu);
}

/**
 * Kick 3, CLRKICK -> the microcode's OCB_KICK03 (CS 0o25522): execute the clear
 * functions coded in X5CLR and acknowledge them.
 *
 * Ported from OctobusND5000Station.cs ExecuteClearFunctions, lines 1994-2022. WHO
 * ASKS, from that routine's remarks: SINTRAN's ST0PSYS (MP-P2-N500.NPL:3759)
 * writes 0o77 into X5CLR, sends CLRKICK and then POLLS X5CLR for zero up to 1000
 * times, calling ERRFATAL if it never clears; LMPCLR (MP-P2-N500.NPL:1222) writes
 * a mask taken from the swapper's message and does not poll. With this kick
 * dropped X5CLR kept its mask for ever and every stop-system took the ERRFATAL
 * branch.
 *
 * THE THREE WRITES are what the real B30 microcode does, observed by executing
 * OCB_KICK03 with X5CLR = 0o77 (same remarks):
 *     CS 025627: [ext+0x12] := 0001   X5CCL := 1   (cache-clear counter)
 *     CS 025536: [ext+0x10] := 0000   X5CLR := 0   (the acknowledge)
 *     CS 025421: [ext+0x0C] := FFFF   X5PRO := -1  (IDLE = "forget process")
 * The mask's cache, data-TSB and dump bits have no further effect because no
 * ND-5000 cache or TSB is modelled - there is nothing to invalidate.
 */
static void nd5000_execute_clear_functions(NdbusNd5000 *nd)
{
    uint32_t ext = ndbus_mailbox_ext_base(&nd->mailbox);
    if (ext == 0u)
    {
        nd_log(nd, "ND-5000 octobus: KICK 3 (CLRKICK) ignored - mailbox extension block not "
                   "configured yet");
        return;
    }

    /* ONE LOCK FOR THE WHOLE READ-MODIFY-WRITE, as the reference takes
     * _mpm.SyncRoot (line 2004): the ND-100 polls X5CLR and must never observe a
     * half-applied acknowledge. ndbus_lock() is that same lock domain here - see
     * ndbus_lock.h. It is not recursive, so nothing below may take it again; the
     * mailbox accessors do not, and the log line is written after the release. */
    ndbus_lock();
    uint16_t mask = ndbus_mailbox_read_ext(&nd->mailbox, NDBUS_MBX_X5CLR_WORD);

    /* X5CCL, the counter SINTRAN reads and compares elsewhere. */
    (void)ndbus_mailbox_write_ext(&nd->mailbox, NDBUS_MBX_X5CCL_WORD, 1u);

    /* X5PRO := -1 = "ND-500 IDLE" - the "forget process" half of the mask. */
    (void)ndbus_mailbox_write_ext(&nd->mailbox, NDBUS_MBX_X5PRO_WORD, 0xFFFFu);

    /* LAST, AND THE ORDER MATTERS: X5CLR := 0 is the ND-100's release signal.
     * Written before the other two it would let SINTRAN proceed while X5CCL and
     * X5PRO still held stale values (line 2014). */
    (void)ndbus_mailbox_write_ext(&nd->mailbox, NDBUS_MBX_X5CLR_WORD, 0u);
    ndbus_unlock();

    char text[160];
    (void)snprintf(text, sizeof(text),
                   "ND-5000 octobus: KICK 3 (CLRKICK) mask=0x%04X -> X5CLR:=0 X5CCL:=1 X5PRO:=-1 "
                   "(ext block 0x%08X)",
                   (unsigned)mask, (unsigned)ext);
    nd_log(nd, text);
}

/**
 * Kick 6, IDLEKICK -> the microcode's OCB_KICK06 (CS 0o25561): forced de-schedule.
 *
 * Ported from OctobusND5000Station.cs GoIdle, lines 2050-2064. WHO ASKS: TER51
 * (MP-P2-N500.NPL:2950), the ND-500 TERMINATE path, sends kick 6 in a loop and
 * leaves ONLY when X5PRO reads -1; otherwise it falls into TER52 ESPTIMOUT. So
 * X5PRO := -1 is the observable, and it is all this does.
 *
 * DELIBERATELY PARTIAL, exactly as the reference is and for its stated reason:
 * the real routine also does CNTXTSAVE, OCB_CLNUP, UNLOCK_QUE and PRNOWR.
 * OCB_CLNUP REQUEUES the message in progress (N5STA := 1) rather than discarding
 * it, so a version that dropped the current message would lose work in a way that
 * shows up much later. Not modelled there, so not modelled here.
 */
static void nd5000_go_idle(NdbusNd5000 *nd)
{
    uint32_t ext = ndbus_mailbox_ext_base(&nd->mailbox);
    if (ext == 0u)
    {
        nd_log(nd, "ND-5000 octobus: KICK 6 (IDLEKICK) ignored - mailbox extension block not "
                   "configured yet");
        return;
    }

    ndbus_lock();
    (void)ndbus_mailbox_write_ext(&nd->mailbox, NDBUS_MBX_X5PRO_WORD, 0xFFFFu);
    ndbus_unlock();

    char text[160];
    (void)snprintf(text, sizeof(text),
                   "ND-5000 octobus: KICK 6 (IDLEKICK) -> X5PRO:=-1 (ext block 0x%08X); "
                   "OCB_CLNUP requeue NOT modelled",
                   (unsigned)ext);
    nd_log(nd, text);
}

/**
 * One kick frame, already known to be a control frame with K set and M clear.
 *
 * Ported from OctobusND5000Station.cs HandleFrame, lines 2399-2523. What is left
 * out is what the functional-CPU lane does not have: LoadAob (the AOB/AIB model),
 * the parked-microword-CPU branch, and the KickReceived and MailboxDoorbell
 * events. For kicks 1 and 2 the reference has two branches - a doorbell
 * subscriber that moves the walk onto the CPU's own thread, and a synchronous
 * walk when there is none (lines 2473-2481). This library has no subscriber
 * mechanism, and its servicer already runs on the thread that delivers the frame,
 * so the synchronous branch is the one ported.
 */
static void nd5000_handle_kick(NdbusNd5000 *nd, uint16_t frame, uint8_t source_station)
{
    /* Broadcast is NOT allowed for kicks (ND-05.020.01 page 336). Rejected before
     * it is counted, as at lines 2402-2408. */
    if ((frame & NDBUS_FRAME_B_BROADCAST) != 0)
    {
        return;
    }

    unsigned kick_number = (unsigned)(frame & 0x3Fu);

    /* Count EVERY decoded kick, enabled or not (line 2413). */
    nd->kick_counts[kick_number]++;

    /* Kicks reach the microprogram only when ENKICK is in force (line 2417). */
    if (!nd->accp.kicks_enabled)
    {
        nd->kicks_dropped_disabled++;
        return;
    }

    switch (kick_number)
    {
    case 1u:
    case 2u:
        /* ACTIVATE. The microcode sends BOTH 1 and 2 here; only N100KICK = 1 has a
         * known NPL sender (ACT52, MP-P2-N500.NPL:3032), but the table is explicit,
         * so 2 must not fall through silently. Nothing happens until a mailbox has
         * been located - the reference's "_mailboxHeaderBase != 0", line 2471. */
        if (nd->start_mess != 0u)
        {
            (void)nd5000_walk_queue(nd);
        }
        break;

    case 3u:
        /* CLRKICK -> OCB_KICK03, the cache-clear protocol. */
        nd5000_execute_clear_functions(nd);
        break;

    case 6u:
        /* IDLEKICK -> OCB_KICK06, forced de-schedule. */
        nd5000_go_idle(nd);
        break;

    default:
        /* 0 and 7-63 land on NOTREC in the real microcode, which REPORTS the
         * unrecognised kick and releases the queue lock; 4 and 5 are OCB_KICK05,
         * which the reference does not implement either. LOGGED UNCONDITIONALLY
         * (lines 2495-2509): swallowing these in silence is how the kick-3 gap
         * survived unnoticed until stop-system was fixed. */
        {
            char text[160];
            (void)snprintf(text, sizeof(text),
                           "ND-5000 octobus: KICK %u from station %oB NOT IMPLEMENTED (microcode "
                           "would run %s)",
                           kick_number, (unsigned)source_station,
                           (kick_number == 4u || kick_number == 5u) ? "OCB_KICK05" : "NOTREC 204");
            nd_log(nd, text);
        }
        break;
    }
}

/* ---- frame routing ---------------------------------------------------------- */

static int nd5000_handle_frame(NdbusStation *station, uint16_t frame, uint8_t source_station,
                               uint16_t *replies)
{
    NdbusNd5000 *nd = (NdbusNd5000 *)station->ctx;
    if (nd == NULL)
    {
        return 0;
    }

    bool control   = (frame & NDBUS_FRAME_C_CONTROL) != 0;
    bool emergency = (frame & NDBUS_FRAME_E_EMERGENCY) != 0;
    bool kick      = (frame & NDBUS_FRAME_K_KICK) != 0;
    bool multibyte = (frame & NDBUS_FRAME_M_MULTIBYTE) != 0;
    bool start     = (frame & NDBUS_FRAME_S_STARTSTOP) != 0;

    /*
     * EMERGENCY FIRST, AND THE CODE IS THE WHOLE INFORMATION BYTE.
     *
     * Ported from RetroCore OctobusND5000Station.cs HandleFrame, which tests
     * `isControl && isEmergency` before anything else and calls
     * `HandleEmergency((byte)(frame & 0xFF))` - the full low byte, not the 4-bit
     * CODE field. The emergency numbers are 241B, 242B and 244B, which do not fit
     * in four bits at all: masking with NDBUS_FRAME_CODE_MASK turns 0xA1 into 1
     * and 0xA2 into 2, and no station could ever recognise them.
     *
     * This branch did not exist here until 26-SEP-2026; both frames fell through
     * to the "accepted and silent" tail. They are the whole of SINTRAN's cold-start
     * CPU probe: CH5CPUPRESENT reads octobus status 100406, waits for bit 3, then
     * sends CMMACLE (241B) and CMACONT (242B) to each station and flags the CPU as
     * SAMSON (PH-P2-OPPSTART.NPL:3893-3943, via $ND_BUSI).
     *
     * An emergency is hardware-decoded and carries NO REPLY - the acknowledge is
     * the bus's, not the station's.
     */
    if (control && emergency)
    {
        uint8_t code       = (uint8_t)(frame & NDBUS_FRAME_DATA_MASK);
        nd->last_emergency = code;

        switch (code)
        {
        case NDBUS_EMERGENCY_MASTER_CLEAR:
            /* 241B: resets the ACCP and the ND-5000 CPU. Buffers and flags go,
             * kicks go back off, and the idle loop is left. */
            nd->master_clears++;
            nd->accp.microprogram_running     = false;
            nd->accp.system_parameters_given  = false;
            nd->accp.parameter_pointer_given  = false;
            nd->accp.kicks_enabled            = false;
            nd->accp_idle                     = false;
            nd->parameter_pointer             = 0;
            ndbus_multibyte_reset(&nd->inbox);
            /* AND THE CPU, after the station's own state: "ResetStation();
             * ResetCpuToIdle();" at OctobusND5000Station.cs lines 2674-2675. The
             * callback must reset the CPU AND park it in the microcode IDLE state.
             * The reference's remark on ResetCpuToIdle (lines 2633-2651) records
             * what a reset without the park does: SINTRAN's CH5CPUPRESENT scan
             * sends 241B to every ND-5000 station at boot, and a CPU left runnable
             * executed garbage from PC 0 and never parked again. */
            if (nd->reset_cpu_to_idle != NULL)
            {
                nd->reset_cpu_to_idle(nd->cpu_hook_ctx);
            }
            nd_log(nd, "ND-5000 octobus: emergency 241B MASTER CLEAR");
            break;

        case NDBUS_EMERGENCY_CONTINUE_ACCP:
            /* 242B: sent by CH5CPUPRESENT right after the master clear, to let
             * the ACCP run its startup sequence. */
            nd->continues++;
            nd->accp_idle = false;
            nd_log(nd, "ND-5000 octobus: emergency 242B CONTINUE ACCP");
            break;

        case NDBUS_EMERGENCY_TERMINATE_ACCP:
            /* 244B: the ACCP program enters its idle loop and the microprogram
             * STOPS. The ND-500 monitor sends this on an ACCP timeout. A following
             * alive check must therefore report "not running". */
            nd->terminates++;
            nd->accp_idle                 = true;
            nd->accp.microprogram_running = false;
            nd_log(nd, "ND-5000 octobus: emergency 244B TERMINATE ACCP");
            break;

        default:
            /* Other emergency opcodes - the R/H interpretation bits of Appendix 2
             * section 2.6 - are not modelled. Counted, named in the log, ignored:
             * guessing at one would be worse than admitting it. */
            nd_log(nd, "ND-5000 octobus: emergency not modelled, ignored");
            break;
        }
        return 0;
    }

    /*
     * THE IDLE GATE. After emergency 244B the ACCP sits in its idle loop, and
     * while it does every frame is dropped EXCEPT the multibyte traffic for OMD 0
     * and OMD 3: their SOMB and EOMB, and the data bytes of a message one of those
     * SOMBs opened.
     *
     * Ported from OctobusND5000Station.cs HandleFrame, lines 2376-2397. Its reason:
     * a terminated ACCP still runs its 68000 communication loop - only the
     * MICROPROGRAM is stopped - so it must still answer ACCP command messages,
     * notably the ALIVE check SINTRAN sends right after a terminate to confirm it
     * worked (no reply reads as "Impossible to terminate ACCP after timeout").
     * Kicks and everything else meant for the microprogram are ignored.
     *
     * The gate sits AFTER the emergency decode, so 241B and 242B still get through
     * to clear the flag; a 066B, 035B or 036B start clears it from inside the
     * OMD-3 traffic the gate lets pass. Until 05-OCT-2026 the flag was written and
     * never read.
     */
    if (nd->accp_idle)
    {
        uint8_t idle_omd       = (uint8_t)(frame & NDBUS_FRAME_CODE_MASK);
        bool    accp_multibyte = control && multibyte &&
                                 (idle_omd == NDBUS_TESTPROTO_OMD || idle_omd == NDBUS_ACCP_OMD);
        bool    accp_body_byte = !control && nd->inbox.open; /* data of an open ACCP message */
        if (!accp_multibyte && !accp_body_byte)
        {
            return 0;
        }
        /* else fall through: answer the ACCP command even while idle */
    }

    if (control && kick && !multibyte)
    {
        /* A kick never has a reply frame: what it causes is seen in shared memory. */
        nd5000_handle_kick(nd, frame, source_station);
        return 0;
    }

    if (control && multibyte)
    {
        /* SOMB (S=1) or EOMB (S=0). The OMD in the low bits says who the message
         * is for; OMD 3 is ACCP device handling (T124, figure 28) and OMD 0 is the
         * Octobus Test Protocol of ndbus_testproto.h. This station answers BOTH,
         * as RetroCore's does - see the note on `testproto` in ndbus_nd5000.h for
         * why one collector can serve the two. */
        uint8_t omd = (uint8_t)(frame & NDBUS_FRAME_CODE_MASK);
        if (omd != NDBUS_ACCP_OMD && omd != NDBUS_TESTPROTO_OMD)
        {
            /* ANOTHER OMD IS THE MICROPROGRAM'S, SOMB AND EOMB ALIKE. The reference
             * forwards both to the microprogram (lines 2582-2588) and returns, so
             * an ACCP message that is being collected STAYS OPEN and is not run.
             * Until 05-OCT-2026 only the SOMB was tested here: an EOMB for any OMD
             * closed whatever message was open and ran it as an ACCP command. The
             * forwarding itself is the AOB model, which this port does not have,
             * so the frame is accepted and nothing more happens to it. */
            return 0;
        }

        if (start)
        {
            ndbus_multibyte_begin(&nd->inbox, source_station);
            return 0;
        }

        /* EOMB: the message is complete. */
        if (!ndbus_multibyte_end(&nd->inbox))
        {
            return 0; /* nothing open, or the body overflowed */
        }
        if (nd->inbox.count < 1)
        {
            return 0; /* an empty body carries no command byte */
        }
        /* THE EOMB FRAME SAYS WHICH PROTOCOL THE MESSAGE BELONGS TO, which is why
         * one collector can serve both OMDs. Ported from RetroCore
         * OctobusND5000Station.cs HandleFrame (lines 2525-2580), which dispatches
         * on the completing frame's OMD rather than on anything remembered from
         * the SOMB.
         *
         * The OMD-0 responder keeps its OWN message count, so `messages_handled`
         * is bumped only on the ACCP path: it counts ACCP commands, and a test
         * protocol message is not one. Counting both here would make the two
         * diagnostics describe the same traffic and neither of them the truth. */
        if (omd == NDBUS_TESTPROTO_OMD)
        {
            return ndbus_testproto_answer(&nd->testproto, &nd->station, nd->inbox.bytes,
                                          nd->inbox.count, replies, NDBUS_MAX_REPLY_FRAMES);
        }

        nd->messages_handled++;
        return run_command(nd, nd->inbox.bytes, nd->inbox.count, replies,
                           NDBUS_MAX_REPLY_FRAMES);
    }

    if (!control)
    {
        /* A data frame: one body byte, in the low 8 bits. */
        (void)ndbus_multibyte_push(&nd->inbox, (uint8_t)(frame & NDBUS_FRAME_DATA_MASK));
        return 0;
    }

    /* A control frame that is none of the above is an ident (C=1, E=K=M=0). The
     * reference hands idents to the microprogram through the AOB (lines
     * 2591-2609), which this port does not model, so it is accepted and silent.
     * Emergencies and kicks are NOT in this bucket - they are handled above. */
    return 0;
}

/**
 * The servicer has written an answer: tell the ND-100 with the GIVEINT interrupt
 * frame. Ported from RetroCore OctobusND5000Station.cs IServicerHost.AnswerWritten.
 *
 * THE MICROCODE'S OWN ARITHMETIC, and it must not be "corrected": the frame word
 * is (LSYSPAR word 1 AND 0x3F00) OR 0x8001, from GIVEINT1 at microcode
 * 025440-025441. Bits 13-8 of a frame are the destination station, so word 1's
 * station byte IS the destination - the frame goes to whoever LSYSPAR named, and
 * the ND-100 is station 1.
 *
 * RETROCORE MEASURES THIS PATH AS UNDELIVERED ON ITS OWN CONFIGURATION, and that
 * is recorded rather than worked around: its LSYSPAR word 1 carries 5OMDNO 3, the
 * expression yields 0x8061 whose destination field is 0, and the fabric drops
 * station 0. Its note reads "737 answers sent, 0 delivered". Both of its
 * workarounds are environment-gated and labelled as not being fixes, so neither
 * is ported. What IS ported is the arithmetic plus the counters that tell the two
 * apart, so a dropped frame here is visible instead of silent.
 */
static void nd5000_answer_written(void *ctx, uint32_t msg_byte)
{
    NdbusNd5000 *nd = (NdbusNd5000 *)ctx;
    (void)msg_byte;

    if (nd->start_mess == 0u)
    {
        nd->giveint_no_mailbox++;
        return;
    }
    if (nd->station.fabric == NULL)
    {
        nd->giveint_no_fabric++;
        return;
    }

    /* NO SHIFT. GIVEINT1 composes the frame as
     *     (LSYSPAR word 1 AND 0x3F00) OR 0x8001
     * and nothing else - verified in the current RetroCore source
     * (OctobusND5000Station.cs:2273). Word 1 holds the ident byte in bits 15-8 and
     * 5OMDNO in bits 7-0, so masking 0x3F00 takes the IDENT byte as the destination
     * station: 0x0108 gives 0x8101 = 100401B, station 1, the ND-100 - the word
     * observed on the live machine.
     *
     * A >> 3 was added here by commit b311a1a to make a one-byte-late capture come
     * out right. Both are now corrected: the capture starts at the ident byte and the
     * arithmetic is the microcode's again. */
    uint16_t frame = (uint16_t)((nd->lsyspar_word1 & 0x3F00u) | 0x8001u);
    nd->last_giveint_frame = frame;
    nd->giveint_frames++;

    uint16_t replies[NDBUS_MAX_REPLY_FRAMES];
    (void)ndbus_fabric_send(nd->station.fabric, nd->station.number, frame, replies);
}

static void nd5000_servicer_log(void *ctx, const char *message)
{
    nd_log((const NdbusNd5000 *)ctx, message);
}

bool ndbus_nd5000_init(NdbusNd5000 *nd, uint8_t station_number, NdbusPool *pool,
                       const NdbusHostOps *host, const NdbusCpuOps *cpu)
{
    if (nd == NULL)
    {
        return false;
    }

    /* 70B..76B and nothing else. A station number is configuration, and an
     * ND-5000 answering at 10B would quietly collide with a SCSI controller
     * rather than fail where someone can see it. */
    if (station_number < NDBUS_STATION_ND5000_FIRST || station_number > NDBUS_STATION_ND5000_LAST)
    {
        return false;
    }

    memset(nd, 0, sizeof(*nd));
    nd->station.number = station_number;
    nd->station.type   = "ND-5000 CPU";
    nd->station.handle = nd5000_handle_frame;
    nd->station.ctx    = nd;
    nd->pool           = pool;
    nd->host           = host;
    nd->cpu            = cpu;
    nd->last_nak_code  = NDBUS_ACCP_ACCEPTED;
    nd->sniff.threshold = NDBUS_X5ACT_LATCH_ON_FIRST;
    /* CPUNO 1 until a mailbox configuration says otherwise - the reference's
     * "private int _cpuNumber = 1", OctobusND5000Station.cs line 949. */
    nd->cpu_number     = 1;
    ndbus_multibyte_reset(&nd->inbox);

    /* An ND-5000 answers the Test Protocol's Get module type with 3 = ACCP, not
     * 1 = Domino controller. RetroCore OctobusND5000Station.cs:702 sets the same
     * value on the same station class. */
    ndbus_testproto_init(&nd->testproto, NDBUS_TESTPROTO_MODULE_ACCP);

    NdbusServicerHost svhost;
    memset(&svhost, 0, sizeof svhost);
    svhost.ctx            = nd;
    svhost.answer_written = nd5000_answer_written;
    svhost.log            = nd5000_servicer_log;
    /* A station with no pool has nothing to service; the servicer stays zeroed
     * and ndbus_nd5000_service_mailbox() refuses because no mailbox is located. */
    if (pool != NULL)
    {
        (void)ndbus_servicer_init(&nd->servicer, pool, &svhost);
    }
    return true;
}

void ndbus_nd5000_reset(NdbusNd5000 *nd)
{
    if (nd == NULL)
    {
        return;
    }
    /* NOT THE 071B CPURES COMMAND. That command clears two guard cells and resets
     * the CPU, and has its own arm in run_command(); this helper clears more and
     * is for an owner that wants a cold station. See ndbus_nd5000.h.
     *
     * The ACCP's own state only. The POOL IS NOT TOUCHED: it is shared memory,
     * the ND-100 owns what is in it, and a reset that wiped it would destroy the
     * other side's data. */
    memset(&nd->accp, 0, sizeof(nd->accp));
    nd->parameter_pointer = 0;
    ndbus_multibyte_reset(&nd->inbox);
    memset(&nd->sniff, 0, sizeof(nd->sniff));
    nd->sniff.threshold = NDBUS_X5ACT_LATCH_ON_FIRST;
    /* THE CONTROL STORE SURVIVES. It is the ND-5000's microcode store, not ACCP
     * state, and the buffer is kept so a reload does not have to allocate again. */
}

/**
 * Record where the mailbox is: the header, this CPU's CPUNO, and with them the
 * extension block at header + CPUNO * 256.
 *
 * Ported from OctobusND5000Station.cs ConfigureMailbox, lines 1395-1403, which
 * stores the three values and checks NOTHING - the caller has already applied its
 * own guard. ndbus_mailbox_attach() is deliberately not used for this: it refuses
 * a CPUNO above 7 and any block whose full 256 bytes do not fit the pool, and both
 * refusals are stricter than the reference, whose guard (line 1457) asks only for
 * the first 17 bytes of the extension block. A block that runs past the end of the
 * pool is still safe to use: every mailbox accessor goes through the bounds-checked
 * pool calls, where a read outside the pool returns 0 and a write is refused.
 */
static void nd5000_configure_mailbox(NdbusNd5000 *nd, uint32_t header_byte, int cpu_number)
{
    nd->mailbox.pool        = nd->pool;
    nd->mailbox.header_byte = header_byte;
    nd->mailbox.cpuno       = cpu_number;
    nd->cpu_number          = cpu_number;
}

bool ndbus_nd5000_mailbox_from_control_store(NdbusNd5000 *nd)
{
    if (nd == NULL || nd->control_store == NULL || nd->pool == NULL)
    {
        return false;
    }

    /* 026B = 0x16 and 025B = 0x15. Both are LARG constants, so the value is the
     * 32-bit field built from halfwords 6 and 7, high half first. */
    uint32_t start_mess = cs_read_larg(nd, 0x16u);
    uint32_t samson_cpu = cs_read_larg(nd, 0x15u);

    if (start_mess == 0u)
    {
        /* The control store is loaded but not patched - leave the mailbox alone
         * rather than attaching one at pool offset 0, which is the global header
         * of nothing. */
        return false;
    }

    /* SAMSON_CPU is the extension-block index. When the cell is unpatched the
     * fallback is the station's current CPUNO - 1 from init, or whatever the last
     * configuration set - exactly as OctobusND5000Station.cs line 1453 has it:
     *     int cpu = samsonCpu != 0 ? (int)samsonCpu : _cpuNumber;
     * Until 05-OCT-2026 this fell back to a number derived from the station's
     * position on the bus (2 for 071B, 3 for 072B ...), which the reference never
     * does: with the cell unpatched it uses block 1 whatever the station. */
    int cpu = (samson_cpu != 0u) ? (int)samson_cpu : nd->cpu_number;

    /* THE GUARD, WORD FOR WORD - lines 1454-1461:
     *     uint header = _mpm.Start + startMess;
     *     uint ext    = header + (uint)(cpu * 256);
     *     if (header < _mpm.Start || ext + 16 >= _mpm.Start + _mpm.Size)  -> ignored
     * in the reference's own units, ND-100 physical byte addresses, and in its own
     * wrapping 32-bit arithmetic: _mpm.Start is the window's ND-100 base and
     * _mpm.Size is the pool size. The first test catches a START_MESS so large
     * that the header address wraps; the second wants bytes 0..16 of the extension
     * block inside the window. Until 05-OCT-2026 this demanded the whole 256-byte
     * block instead. */
    uint32_t window_start = nd5000_window_base(nd);
    uint32_t header       = window_start + start_mess;
    uint32_t ext          = header + ((uint32_t)cpu * NDBUS_MBX_STRIDE_BYTES);
    if (header < window_start || (ext + 16u) >= (window_start + nd->pool->size))
    {
        nd_log(nd, "ND-5000 ACCP: START_MESS mailbox base is outside the window - ignored");
        return false;
    }

    /* START_MESS is a window BYTE offset, and the pool is the window, so the pool
     * offset of the header is START_MESS itself. */
    nd5000_configure_mailbox(nd, start_mess, cpu);

    nd->start_mess = start_mess;
    nd->samson_cpu = samson_cpu;
    /* The servicer needs the header to do the GIVEINT ring insert. Until it has
     * one it writes N5STA only, which is why this is set here and not at init. */
    (void)ndbus_servicer_set_header(&nd->servicer, start_mess);
    /* The control store is authoritative, so the doorbell sniff must not go on
     * hunting for a transition and latch onto noise at some other offset. */
    nd->sniff.latched          = true;
    nd->sniff.have_candidate   = true;
    nd->sniff.candidate_offset = ndbus_mailbox_ext_base(&nd->mailbox) +
                                 (NDBUS_MBX_X5ACT_WORD * 2u);
    nd_log(nd, "ND-5000 ACCP: mailbox located from the control store (START_MESS)");

    /* NOTHING ELSE IS READ HERE. The segment table, the context block area and the
     * CPU identity come out of the same store, but at the 066B/035B/036B start and
     * independently of this routine - see the arm in run_command(). */
    return true;
}

bool ndbus_nd5000_try_get_context_block_area_base(NdbusNd5000 *nd, uint32_t *byte_base)
{
    if (nd == NULL || byte_base == NULL)
    {
        return false;
    }

    /* Ported from OctobusND5000Station.cs IServicerHost.TryGetContextBlockAreaBase,
     * lines 2181-2189:
     *     byteBase = HaveControlStore ? ReadControlStoreLarg(0x10) : 0u;
     *     if (byteBase != 0 && LoadedContextBlockBase == 0) LoadedContextBlockBase = byteBase;
     *     return byteBase != 0;
     * The cell is read EVERY time, not taken from what the start cached, which is
     * the point: SINTRAN may patch 0o20 after the start command has already run.
     *
     * WHAT IS STILL MISSING, AND WHERE. The reference's servicer asks this of its
     * host whenever its own cached base is zero (Nd500MicrocodeServicer.cs lines
     * 1329, 2113, 3443 and 3735). NdbusServicerHost has no callback for it, and
     * ndbus_servicer.c tests context_area_base alone, so nothing calls this yet. */
    uint32_t value = cs_read_larg(nd, 0x10u); /* 0o20 OFFSET; 0 with no control store */
    *byte_base = value;
    if (value != 0u && nd->context_area == 0u)
    {
        nd->context_area = value;
    }
    return value != 0u;
}

bool ndbus_nd5000_set_cpu_hooks(NdbusNd5000 *nd, void (*reset_cpu_to_idle)(void *ctx),
                                void (*apply_init_state)(void *ctx), void *ctx)
{
    if (nd == NULL)
    {
        return false;
    }

    nd->reset_cpu_to_idle = reset_cpu_to_idle;
    nd->apply_init_state  = apply_init_state;
    nd->cpu_hook_ctx      = ctx;
    return true;
}

void ndbus_nd5000_destroy(NdbusNd5000 *nd)
{
    if (nd == NULL)
    {
        return;
    }
    free(nd->control_store);
    nd->control_store = NULL;
    memset(nd, 0, sizeof(*nd));
}

void ndbus_nd5000_set_sniff_threshold(NdbusNd5000 *nd, uint32_t threshold)
{
    if (nd == NULL)
    {
        return;
    }
    nd->sniff.threshold = threshold;

    if (threshold >= 2u)
    {
        /* Said out loud at the moment it is set, because the failure is silence:
         * the sniff never latches and the machine just looks idle. */
        char text[400];
        (void)snprintf(text, sizeof(text),
                       "ND-5000 octobus: sniff repeat threshold %u CANNOT BE MET by the genuine "
                       "X5ACT doorbell. XMSINIT sets X5ACT to -1 and the microcode re-arms it to "
                       "1 (microword 0o24722, IDLE_2), so the -1 to 0 signature occurs ONCE per "
                       "XMSINIT. The sniff will not latch and the CPU will look idle rather than "
                       "report an error. Use 0 or 1 unless you are deliberately suppressing "
                       "self-discovery.",
                       (unsigned)threshold);
        nd_log(nd, text);
    }
}

bool ndbus_nd5000_sniff_before_write16(NdbusNd5000 *nd, uint32_t offset, uint16_t value)
{
    if (nd == NULL || nd->pool == NULL || nd->sniff.latched)
    {
        return false;
    }

    /* The signature is the TRANSITION 0xFFFF -> 0, so the cell's previous value
     * decides, not the value being written - which is why this must be called
     * BEFORE the write lands. Called afterwards, the "previous" value read here
     * is the new one and no transition is ever seen. */
    uint16_t previous = ndbus_pool_read16(nd->pool, offset);
    if (!(previous == 0xFFFFu && value == 0u))
    {
        return false;
    }

    if (nd->sniff.have_candidate && nd->sniff.candidate_offset == offset)
    {
        nd->sniff.transitions++;
    }
    else
    {
        nd->sniff.have_candidate   = true;
        nd->sniff.candidate_offset = offset;
        nd->sniff.transitions      = 1;
    }

    /* 0 and 1 both mean "latch on the first transition" - which is the only
     * threshold the genuine doorbell can satisfy. */
    uint32_t needed = (nd->sniff.threshold < 2u) ? 1u : nd->sniff.threshold;
    if (nd->sniff.transitions < needed)
    {
        return false;
    }

    nd->sniff.latched = true;
    return true;
}

bool ndbus_nd5000_set_process_host(NdbusNd5000 *nd,
                                   bool (*start)(void *ctx, uint32_t msg_byte, uint16_t micfu,
                                                 uint32_t ctx_byte))
{
    if (nd == NULL)
    {
        return false;
    }

    nd->servicer.host.start_process = start;
    nd->servicer.host.ctx = nd;
    return true;
}

bool ndbus_nd5000_set_data_reader(NdbusNd5000 *nd,
                                  bool (*read)(void *ctx, uint32_t logical_address,
                                               uint8_t *destination, uint32_t count))
{
    if (nd == NULL)
    {
        return false;
    }

    nd->servicer.host.read_nd500_data_bytes = read;
    nd->servicer.host.ctx = nd;
    return true;
}

bool ndbus_nd5000_set_dit_declarer(NdbusNd5000 *nd,
                                   void (*declare)(void *ctx, uint32_t base))
{
    if (nd == NULL)
    {
        return false;
    }

    nd->servicer.host.declare_dit_base = declare;
    nd->servicer.host.ctx = nd;
    return true;
}

bool ndbus_nd5000_service_mailbox(NdbusNd5000 *nd)
{
    if (nd == NULL || nd->pool == NULL || nd->start_mess == 0u)
    {
        return false;
    }

    nd->service_polls++;

    /* X5ACT is the doorbell and ZERO is the rung value. -1 is XMSINIT's idle
     * value and 1 is the re-armed value; both mean nothing pending. */
    uint16_t x5act = ndbus_mailbox_read_ext(&nd->mailbox, NDBUS_MBX_X5ACT_WORD);
    if (x5act != 0u)
    {
        return false;
    }

    nd->service_activations++;

    /* Re-arm BEFORE walking, with 1 - which is what IDLE_2 writes, not -1. The
     * order matters: a message queued while the walk is running must leave the
     * doorbell rung again rather than be absorbed into the iteration that is
     * already past its queue read. */
    (void)ndbus_mailbox_write_ext(&nd->mailbox, NDBUS_MBX_X5ACT_WORD,
                                  NDBUS_MBX_X5ACT_REARM);

    /* X5BEX, the ex-queue head, is 32 bits at extension-block words 0-1, high
     * half first, and holds a WINDOW-RELATIVE BYTE offset. -1 is an empty chain;
     * 0 is uninitialised and is refused as an emulator guard rather than walked
     * into the global header. */
    uint32_t ext = ndbus_mailbox_ext_base(&nd->mailbox);
    uint32_t bex_hi = ndbus_pool_read16(nd->pool, ext + NDBUS_MBX_X5BEX_WORD * 2u);
    uint32_t bex_lo = ndbus_pool_read16(nd->pool, ext + (NDBUS_MBX_X5BEX_WORD + 1u) * 2u);
    uint32_t bex = (bex_hi << 16) | bex_lo;

    if (bex == 0xFFFFFFFFu || bex == 0u)
    {
        return false;
    }

    return ndbus_servicer_process_chain(&nd->servicer, bex & 0xFFFFFFu);
}
