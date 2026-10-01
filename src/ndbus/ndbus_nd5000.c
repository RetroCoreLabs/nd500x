/*
 * ndbus_nd5000.c - the ND-5000's octobus station
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include <stdio.h>
#include <string.h>

#include "ndbus_nd5000.h"

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
 * Messack, with any return parameters after it.
 *
 * T125: "If a command requires a response with returned data via octobus (e.g.
 * READ MIR), this data is sent directly after Messack in the same multibyte
 * message. (Messack is sent as a multibyte message even if there are no return
 * parameters.)"
 *
 * `params` / `param_count` are those return bytes; pass 0 for a bare Messack.
 */
/*
 * The whole multibyte message - envelope and all - is built by
 * ndbus_multibyte_build() in ndbus_multibyte.h, which is shared with the OMD-0
 * Test Protocol responder. There is one implementation of the envelope because
 * there is one envelope.
 */
/*
 * Messack: leading status byte 0 = CMACK/MFACK, "OK / alive / self-test passed".
 *
 * Ported from RetroCore OctobusND5000Station.cs SendAccpMessack: the payload is
 * an all-zero status WORD, and a command that returns data puts the same 0x00 ack
 * byte in front of its data bytes (SendAccpData). Neither the 68000 self-test nor
 * the control-store checksum is modelled - a status-0 Messack is all SINTRAN
 * observes.
 *
 * reply_omd is the SOURCE OMD out of the command message, never a constant: that
 * is the OMD the sender is listening on.
 */
/*
 * How many return bytes a Messack can carry. This is the buffer below, not a
 * documented ACCP limit: T125 says returned data follows Messack in the same
 * multibyte message and does not give a maximum. It used to be enforced
 * indirectly by NDBUS_MAX_REPLY_FRAMES being 16, which is no longer the frame
 * limit, so it is spelled out here rather than left to a constant that has
 * nothing to do with it.
 */
#define ACCP_MAX_RETURN_BYTES 16

static int build_messack(uint8_t station, uint8_t reply_omd, const uint8_t *params,
                         int param_count, uint16_t *replies, int max)
{
    uint8_t payload[1 + ACCP_MAX_RETURN_BYTES];

    if (param_count < 0 || param_count > (int)sizeof(payload) - 1)
    {
        return 0;
    }

    payload[0] = 0x00; /* MFACK */
    for (int i = 0; i < param_count; i++)
    {
        payload[1 + i] = params[i];
    }

    /* A bare acknowledge is the all-zero status word, so it carries a second
     * zero byte; an acknowledge that returns data carries the data instead. */
    int count = (param_count > 0) ? (1 + param_count) : 2;
    if (param_count == 0)
    {
        payload[1] = 0x00;
    }

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
    uint8_t payload[3];
    int     count;

    payload[0] = 0xFF; /* MFNACK */
    payload[1] = (uint8_t)(nak_code & 0xFF);

    if (short_form)
    {
        count = 2;
    }
    else
    {
        payload[2] = 0x00; /* ASTS */
        count      = 3;
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
    /* Room for the envelope (4 frames) and the leading ack byte, and no more
     * return bytes than a Messack can carry. */
    if (count > max - 5)
    {
        count = max - 5;
    }
    if (count > ACCP_MAX_RETURN_BYTES)
    {
        count = ACCP_MAX_RETURN_BYTES;
    }
    if (count < 0)
    {
        count = 0;
    }
    return build_messack(nd->station.number, reply_omd, (count > 0) ? &params[1] : NULL, count,
                         replies, max);
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
        /* T126: three 16-bit words - error station/OMD, host station/OMD, spare.
         * The values are the microprogram's business; what the guards care about
         * is that the command arrived. */
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

    case NDBUS_ACCP_STARTMIC:
        nd->accp.microprogram_running = true;
        break;

    case NDBUS_ACCP_STOPMIC:
        nd->accp.microprogram_running = false;
        break;

    case NDBUS_ACCP_ENKICK:
        nd->accp.kicks_enabled = true;
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
         * OMD 3 is ACCP device handling (T124, figure 28). */
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
        /* EOMB: the message is complete, and ITS OWN OMD FIELD says which protocol
         * it belongs to - read here rather than remembered from the SOMB, which is
         * what RetroCore OctobusND5000Station.cs HandleFrame does: the omd comes
         * out of the frame in both branches. */
        uint8_t omd = (uint8_t)(frame & NDBUS_FRAME_CODE_MASK);
        if (omd != NDBUS_ACCP_OMD && omd != NDBUS_TESTPROTO_OMD)
        {
            return 0; /* the microprogram's message, not this station's */
        }
        if (!ndbus_multibyte_end(&nd->inbox))
        {
            return 0; /* nothing open, or the body overflowed */
        }
        if (nd->inbox.count < 1)
        {
            return 0; /* an empty body carries no command byte */
        }

        if (omd == NDBUS_TESTPROTO_OMD)
        {
            /* The Octobus Test Protocol. NOT counted in messages_handled, which is
             * the ACCP command counter that the existing tests read. */
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
    /* Module type 3 = ACCP: this station IS the ACCP baby card, which is what
     * RetroCore OctobusND5000Station.cs sets at line 702. */
    ndbus_testproto_init(&nd->testproto, NDBUS_TESTPROTO_MODULE_ACCP);
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
