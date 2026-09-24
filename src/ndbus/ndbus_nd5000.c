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
static int build_messack(uint8_t station, const uint8_t *params, int param_count,
                         uint16_t *replies, int max)
{
    if (max < 1)
    {
        return 0;
    }
    (void)station;

    /* SOMB..EOMB around the body. The station field is filled in by the fabric
     * on delivery, so it is left zero here. */
    int n = 0;
    replies[n++] = (uint16_t)(NDBUS_FRAME_C_CONTROL | NDBUS_FRAME_M_MULTIBYTE |
                              NDBUS_FRAME_S_STARTSTOP | (uint16_t)NDBUS_ACCP_OMD);

    for (int i = 0; i < param_count && n < max; i++)
    {
        replies[n++] = (uint16_t)params[i];
    }
    return n;
}

static int build_messnak(int nak_code, uint16_t *replies, int max)
{
    if (max < 1)
    {
        return 0;
    }
    /* Byte 0 is the command error code; bytes 1 and 2 are ASTS lower then upper.
     * The short form (code 13 from arm 0x0D) carries no status bytes at all. */
    uint8_t code = (uint8_t)(nak_code & 0xFF);
    replies[0]   = (uint16_t)(NDBUS_FRAME_C_CONTROL | NDBUS_FRAME_M_MULTIBYTE |
                            (uint16_t)code);
    return 1;
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
static int accp_vparp(NdbusNd5000 *nd, uint16_t *replies, int max)
{
    uint32_t word = ndbus_pool_read32(nd->pool, nd->parameter_pointer);
    uint8_t  reply[4];
    reply[0] = (uint8_t)(word >> 24u);
    reply[1] = (uint8_t)(word >> 16u);
    reply[2] = (uint8_t)(word >> 8u);
    reply[3] = (uint8_t)(word & 0xFFu);
    return build_messack(nd->station.number, reply, 4, replies, max);
}

/**
 * @brief ECHO TEST: return the test pattern that was sent.
 *
 * T126: "Returns the test pattern." Direct parameters are a count byte followed
 * by that many test bytes, and the Messack carries them back. The command exists
 * to prove the link works at all, so echoing a fixed pattern instead of the one
 * that arrived would prove nothing.
 *
 * @param nd      The station.
 * @param body    Message body; body[0] is the command, body[1] the count.
 * @param length  Body length in bytes.
 * @param replies Reply frames to fill.
 * @param max     Room in replies.
 * @return Number of reply frames written: Messack plus the echoed bytes.
 */
static int accp_echo(NdbusNd5000 *nd, const uint8_t *body, int length, uint16_t *replies, int max)
{
    int count = body[1];
    if (count > (length - 2))
    {
        count = length - 2; /* the sender said more than it sent */
    }
    if (count > NDBUS_MAX_REPLY_FRAMES - 1)
    {
        count = NDBUS_MAX_REPLY_FRAMES - 1;
    }
    return build_messack(nd->station.number, (count > 0) ? &body[2] : NULL, count, replies, max);
}

/*
 * The command itself. Only the state the guards read is maintained here; the
 * commands that move real data (the control store, the multiport test) are not
 * implemented and say so rather than pretending to have worked.
 */
static int run_command(NdbusNd5000 *nd, const uint8_t *body, int length, uint16_t *replies,
                       int max)
{
    uint8_t command = body[0];
    nd->last_command = command;

    /*
     * LENGTH IS CHECKED BEFORE THE GUARDS for LOCSD and DCSD only, and a message
     * that is too short gets NO REPLY AT ALL - not a Messnak. A caller that
     * treats silence as an error reports a fault the real card does not report.
     */
    int needed = ndbus_accp_min_parameter_bytes(command);
    if (ndbus_accp_length_checked_first(command) && (length - 1) < needed)
    {
        nd_log(nd, "ND-5000 ACCP: short LOCSD/DCSD - no reply, exactly as the card");
        return 0;
    }

    int verdict = ndbus_accp_evaluate(command, &nd->accp);
    if (verdict != NDBUS_ACCP_ACCEPTED)
    {
        nd->messnaks++;
        nd->last_nak_code = verdict;
        return build_messnak(verdict, replies, max);
    }

    /* Every other command needs its parameters too; below the measured length
     * the card also stays silent. */
    if ((length - 1) < needed)
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
        return accp_vparp(nd, replies, max);

    case NDBUS_ACCP_ECHO:
        nd->messacks++;
        nd->last_nak_code = NDBUS_ACCP_ACCEPTED;
        return accp_echo(nd, body, length, replies, max);

    case NDBUS_ACCP_LPARP:
        /* T128: "The address of the parameter area in the MFbus memory is
         * given." Four bytes, most significant first (T124). */
        nd->parameter_pointer = ((uint32_t)body[1] << 24u) | ((uint32_t)body[2] << 16u) |
                                ((uint32_t)body[3] << 8u) | (uint32_t)body[4];
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
    return build_messack(nd->station.number, NULL, 0, replies, max);
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
    bool multibyte = (frame & NDBUS_FRAME_M_MULTIBYTE) != 0;
    bool start     = (frame & NDBUS_FRAME_S_STARTSTOP) != 0;

    if (control && multibyte && start)
    {
        /* SOMB. The destination OMD in the low bits says who the message is for;
         * OMD 3 is ACCP device handling (T124, figure 28). */
        uint8_t omd = (uint8_t)(frame & NDBUS_FRAME_CODE_MASK);
        if (omd != NDBUS_ACCP_OMD)
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
     * ident. Accepted and silent; the station has nothing to say about it yet. */
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

bool ndbus_nd5000_sniff_write16(NdbusNd5000 *nd, uint32_t offset, uint16_t value)
{
    if (nd == NULL || nd->pool == NULL || nd->sniff.latched)
    {
        return false;
    }

    /* The signature is the TRANSITION 0xFFFF -> 0, so the cell's previous value
     * decides, not the value being written. */
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
