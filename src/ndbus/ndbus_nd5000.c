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

#include "ndbus_cpunum.h"
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
    uint8_t payload[1 + 16];

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
    uint32_t word = ndbus_pool_read32(nd->pool, nd->parameter_pointer);
    uint8_t  reply[4];
    reply[0] = (uint8_t)(word >> 24u);
    reply[1] = (uint8_t)(word >> 16u);
    reply[2] = (uint8_t)(word >> 8u);
    reply[3] = (uint8_t)(word & 0xFFu);
    return build_messack(nd->station.number, reply_omd, reply, 4, replies, max);
}

/**
 * @brief ECHO TEST: return the test pattern that was sent.
 *
 * T126: "Returns the test pattern." Direct parameters are a count byte followed
 * by that many test bytes, and the Messack carries them back. The command exists
 * to prove the link works at all, so echoing a fixed pattern instead of the one
 * that arrived would prove nothing.
 *
 * @param nd          The station.
 * @param reply_omd   The OMD the sender listens on (message byte 0).
 * @param params      The command's PARAMETERS: params[0] is the count, then the
 *                    pattern. The message header and the command byte are already
 *                    behind them.
 * @param param_count How many parameter bytes arrived.
 * @param replies     Reply frames to fill.
 * @param max         Room in replies.
 * @return Number of reply frames written: Messack plus the echoed bytes.
 */
static int accp_echo(NdbusNd5000 *nd, uint8_t reply_omd, const uint8_t *params, int param_count,
                     uint16_t *replies, int max)
{
    if (param_count < 1)
    {
        return 0;
    }

    int count = params[0];
    if (count > (param_count - 1))
    {
        count = param_count - 1; /* the sender said more than it sent */
    }
    /* Room for the envelope (4 frames) and the leading ack byte. */
    if (count > NDBUS_MAX_REPLY_FRAMES - 5)
    {
        count = NDBUS_MAX_REPLY_FRAMES - 5;
    }
    return build_messack(nd->station.number, reply_omd, (count > 0) ? &params[1] : NULL, count,
                         replies, max);
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
 * (line 3134). Six bytes, as a TRAP_OCBM multibyte message:
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
 * The destination is the runtime-allocated 5OMDNO out of LSYSPAR S5, never a
 * constant, and the SOURCE OMD is 4 - not the 3 an ordinary ACCP reply uses.
 */
static void nd5000_send_microprogram_model_report(NdbusNd5000 *nd)
{
    if (nd == NULL || nd->station.fabric == NULL || nd->control_store == NULL)
    {
        nd->report_no_store++;
        return;
    }

    uint16_t version   = cs_read_halfword(nd, 1u, 7u);
    uint8_t  cpu_model = (uint8_t)(cs_read_halfword(nd, 7u, 7u) & 0xFFu);

    const uint8_t report[6] = {
        0x82u, 0x01u, cpu_model, cpu_model,
        (uint8_t)(version >> 8), (uint8_t)(version & 0xFFu),
    };

    uint8_t  dest_omd = (uint8_t)((nd->lsyspar_word1 >> 8) & 0x3Fu);
    uint16_t frames[NDBUS_MAX_REPLY_FRAMES];
    int      n = ndbus_multibyte_build(nd->station.number, dest_omd, 4u, report,
                                       (int)sizeof report, frames, NDBUS_MAX_REPLY_FRAMES);
    if (n <= 0)
    {
        return;
    }
    nd->model_reports++;
    nd->last_model_report_model = cpu_model;
    nd->last_model_report_version = version;
    for (int i = 0; i < n; i++)
    {
        uint16_t replies[NDBUS_MAX_REPLY_FRAMES];
        (void)ndbus_fabric_send(nd->station.fabric, nd->station.number, frames[i], replies);
    }
}


/**
 * @brief LOCSM 023B: DMA one control-store page out of the parameter area.
 *
 * The parameter area, at the pool offset LPARP gave, holds word0 = N, the number
 * of 128-bit microwords in this pulse; word1 = the control-store microword
 * address to load them at; then N * 8 halfwords of microword data.
 *
 * @param nd The station. Its parameter pointer is already known to be set - the
 *           guard table in ndbus_accp.c refuses LOCSM without one.
 * @return Nothing. A pulse with N = 0, or one that runs off the end of the
 *         control store, copies what fits and stops.
 */
static void accp_load_control_store(NdbusNd5000 *nd)
{
    uint32_t pb       = nd->parameter_pointer;
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
    uint32_t pb      = nd->parameter_pointer;
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
        nd->lsyspar_word1 = (uint16_t)(((uint16_t)params[0] << 8u) | params[1]);
        nd->accp.system_parameters_given = true;
        break;

    case NDBUS_ACCP_VPARP:
        nd->messacks++;
        nd->last_nak_code = NDBUS_ACCP_ACCEPTED;
        return accp_vparp(nd, reply_omd, replies, max);

    case NDBUS_ACCP_ECHO:
        nd->messacks++;
        nd->last_nak_code = NDBUS_ACCP_ACCEPTED;
        return accp_echo(nd, reply_omd, params, param_count, replies, max);

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
        nd->accp_idle = false;
        break;

    case NDBUS_ACCP_STOPMIC:
        nd->accp.microprogram_running = false;
        break;

    case NDBUS_ACCP_ENKICK:
        nd->accp.kicks_enabled = true;
        /* ENKICK is the ACCP-to-microprogram handoff, and it is where the mailbox
         * has to be known: the control store is loaded by now, so the patched
         * START_MESS and SAMSON_CPU cells can be read instead of guessed. A false
         * return is not an error here - a machine whose microcode was never
         * patched simply has no mailbox to attach. */
        (void)ndbus_nd5000_mailbox_from_control_store(nd);
        /* AND THE MODEL/VERSION REPORT ON TOP OF THE ACKNOWLEDGE. RetroCore sends it
         * from this arm when the microprogram is running; see the routine's own
         * comment for why the bare ack is not enough. */
        if (nd->accp.microprogram_running)
        {
            nd5000_send_microprogram_model_report(nd);
        }
        break;

    case NDBUS_ACCP_DISKICK:
        nd->accp.kicks_enabled = false;
        break;

    case NDBUS_ACCP_CPURES:
        ndbus_nd5000_reset(nd);
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

    if (control && multibyte && start)
    {
        /* SOMB. The destination OMD in the low bits says who the message is for;
         * OMD 3 is ACCP device handling (T124, figure 28) and OMD 0 is the
         * Octobus Test Protocol of ndbus_testproto.h. This station answers BOTH,
         * as RetroCore's does - see the note on `testproto` in ndbus_nd5000.h for
         * why one collector can serve the two. */
        uint8_t omd = (uint8_t)(frame & NDBUS_FRAME_CODE_MASK);
        if (omd != NDBUS_ACCP_OMD && omd != NDBUS_TESTPROTO_OMD)
        {
            /* Another OMD is the microprogram's, not the ACCP's. Not an error -
             * and not this station's message either. */
            return 0;
        }
        ndbus_multibyte_begin(&nd->inbox, source_station);
        return 0;
    }

    if (control && multibyte && !start)
    {
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
         * OctobusND5000Station.cs HandleFrame, which dispatches on the completing
         * frame's OMD rather than on anything remembered from the SOMB.
         *
         * The OMD-0 responder keeps its OWN message count, so `messages_handled`
         * is bumped only on the ACCP path: it counts ACCP commands, and a test
         * protocol message is not one. Counting both here would make the two
         * diagnostics describe the same traffic and neither of them the truth. */
        if ((uint8_t)(frame & NDBUS_FRAME_CODE_MASK) == NDBUS_TESTPROTO_OMD)
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

    /* A control frame that is not part of a multibyte message - a kick or an
     * ident. Accepted and silent; the station has nothing to say about it yet.
     * Emergencies are NOT in this bucket any more - they are handled at the top,
     * which is where they belong. */
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
    /* The ACCP's own state only. The POOL IS NOT TOUCHED: it is shared memory,
     * the ND-100 owns what is in it, and a CPU reset that wiped it would destroy
     * the other side's data. */
    memset(&nd->accp, 0, sizeof(nd->accp));
    nd->parameter_pointer = 0;
    ndbus_multibyte_reset(&nd->inbox);
    memset(&nd->sniff, 0, sizeof(nd->sniff));
    nd->sniff.threshold = NDBUS_X5ACT_LATCH_ON_FIRST;
    /* THE CONTROL STORE SURVIVES. It is the ND-5000's microcode store, not ACCP
     * state, and the buffer is kept so a reload does not have to allocate again.
     * On the live machine CPURES arrives BEFORE the first LOCSM pulse, so nothing
     * observed here depends on the choice - said out loud rather than left as an
     * accident of where the memset stops. */
}

/** Read a control-store cell's LARG field: the 32-bit value in halfwords 6 and 7,
 *  high half first.
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
 * Take the pointers SINTRAN patched into the control store, at microprogram start.
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
 *   word 1     LARG halfword 7: the microprogram version 3RMICV answers.
 *   word 7     halfword 7 low byte: the model byte that selects CPUPAR.
 *
 * Ported from RetroCore OctobusND5000Station.cs LoadMmsPointersFromControlStore and
 * DeriveCpuIdentityFromControlStore.
 */
static void nd5000_take_patched_cs_pointers(NdbusNd5000 *nd)
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

    uint16_t version = cs_read_halfword(nd, 1u, 7u);
    uint8_t  model_byte = (uint8_t)(cs_read_halfword(nd, 7u, 7u) & 0xFFu);
    (void)ndbus_servicer_set_cpu_identity(&nd->servicer, version,
                                         cpupar_for_model_byte(model_byte));
}

bool ndbus_nd5000_mailbox_from_control_store(NdbusNd5000 *nd)
{
    if (nd == NULL || nd->control_store == NULL)
    {
        return false;
    }

    /* 026B = 0x16 and 025B = 0x15. Both are LARG constants, so the value is the
     * 32-bit field built from halfwords 6 and 7, high half first. */
    uint32_t start_mess = ((uint32_t)cs_read_halfword(nd, 0x16u, 6u) << 16u) |
                          (uint32_t)cs_read_halfword(nd, 0x16u, 7u);
    uint32_t samson_cpu = ((uint32_t)cs_read_halfword(nd, 0x15u, 6u) << 16u) |
                          (uint32_t)cs_read_halfword(nd, 0x15u, 7u);

    if (start_mess == 0u)
    {
        /* The control store is loaded but not patched - leave the mailbox alone
         * rather than attaching one at pool offset 0, which is the global header
         * of nothing. */
        return false;
    }

    /* SAMSON_CPU is the extension-block index. When the cell is unpatched, fall
     * back to this station's own CPUNO, which is what RetroCore does. */
    int cpuno = (samson_cpu != 0u) ? (int)samson_cpu
                                   : ndbus_cpu_mailbox_cpuno(nd->station.number);

    /* THE WHOLE EXTENSION BLOCK MUST FIT, not just its first word: a base that
     * lands near the end of the pool would give reads that return 0 and writes
     * that are refused, which reads exactly like a microprogram that never
     * answers. */
    uint32_t ext = start_mess + ((uint32_t)cpuno * NDBUS_MBX_STRIDE_BYTES);
    if (!ndbus_pool_contains(nd->pool, ext, NDBUS_MBX_STRIDE_BYTES))
    {
        nd_log(nd, "ND-5000 ACCP: START_MESS mailbox base is outside the pool - ignored");
        return false;
    }

    if (!ndbus_mailbox_attach(&nd->mailbox, nd->pool, start_mess, cpuno))
    {
        nd_log(nd, "ND-5000 ACCP: the START_MESS mailbox base was refused");
        return false;
    }

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

    /* The same patched store carries the segment table, the context block area and
     * the CPU identity. Taken here because this runs at ENKICK, which is the
     * microprogram start - the point RetroCore reads them at. */
    nd5000_take_patched_cs_pointers(nd);
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
