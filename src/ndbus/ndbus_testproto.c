/*
 * ndbus_testproto.c - the OMD-0 Octobus Test Protocol responder
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * Ported from RetroCore Emulated.HW/ND/CPU/NDBUS/NDBusOctobus.cs. The C# line
 * number of each reply layout is cited at the arm that implements it, because
 * that is where the layout came from and there is no second source for most of
 * them.
 */

#include <stdio.h>
#include <string.h>

#include "ndbus_multibyte.h"
#include "ndbus_testproto.h"

/*
 * A request payload word, read out of a collected message.
 *
 * The message is [ source OMD ][ byte count ][ payload... ], so payload word w
 * starts at body offset 2 + 2*w. Payload words are big-endian (high byte first).
 *
 * Ported from NDBusOctobus.cs TryGetRequestWord (line 655). A word the message is
 * too short to carry is REFUSED rather than read as zero: a zero read there would
 * turn a truncated echo request into a valid echo of the wrong pattern.
 */
static bool request_word(const uint8_t *message, int length, int word_index, uint16_t *out)
{
    const int offset = 2 + (word_index * 2);
    if (offset + 1 >= length)
    {
        return false;
    }
    *out = (uint16_t)(((uint16_t)message[offset] << 8) | (uint16_t)message[offset + 1]);
    return true;
}

/* Store one payload word big-endian. TPE stores reply bytes high byte first
 * (NDBusOctobus.cs PutReplyWord, line 621, citing ram:d35d-d36a). */
static void put_word(uint8_t *payload, int word_index, uint16_t value)
{
    payload[word_index * 2]       = (uint8_t)(value >> 8);
    payload[(word_index * 2) + 1] = (uint8_t)(value & 0xFFu);
}

/* Store an ASCII field, space-padded or truncated to byte_length bytes
 * (NDBusOctobus.cs PutReplyAscii, line 628). ASCII only: the ND toolchain has no
 * idea what a multi-byte character is. */
static void put_ascii(uint8_t *payload, int word_index, const char *text, int byte_length)
{
    const int offset = word_index * 2;
    const int len    = (text != NULL) ? (int)strlen(text) : 0;
    for (int i = 0; i < byte_length; i++)
    {
        payload[offset + i] = (i < len) ? (uint8_t)text[i] : (uint8_t)' ';
    }
}

/*
 * The fixed 4-word reply header every reply starts with
 * (NDBusOctobus.cs BuildTestProtocolHeader, line 613):
 *
 *     word 0   0x71C7 magic
 *     word 1   request command + 1
 *     word 2   the answering station number
 *     word 3   status: 0 Ok, 1 illegal register function, 2 illegal command code
 *
 * Returns the payload BYTE count, which is (4 + extra_words) * 2.
 */
static int build_header(uint8_t *payload, int payload_max, uint8_t station, uint16_t command,
                        uint16_t status, int extra_words)
{
    const int bytes = (4 + extra_words) * 2;
    if (extra_words < 0 || bytes > payload_max)
    {
        return 0;
    }
    memset(payload, 0, (size_t)bytes);
    put_word(payload, 0, NDBUS_TESTPROTO_MAGIC);
    put_word(payload, 1, (uint16_t)(command + 1u));
    put_word(payload, 2, (uint16_t)station);
    put_word(payload, 3, status);
    return bytes;
}

void ndbus_testproto_init(NdbusTestProto *tp, uint16_t module_type)
{
    if (tp == NULL)
    {
        return;
    }
    memset(tp, 0, sizeof(*tp));
    tp->module_type = module_type;

    /*
     * UNVERIFIED placeholders, carried over from the C# reference (NDBusOctobus.cs
     * lines 578-604), which records the same doubt. 68000 is the MC68000 the
     * station firmware runs on; the two strings and the version number are what
     * an emulator has to say when nothing records what the real card says. One
     * live capture of a DOMINO station answering command 0x0016 and 0x0018 would
     * replace all four.
     */
    tp->processor_type = 68000u;
    tp->test_version   = 1u;
    (void)snprintf(tp->opcom_version, sizeof(tp->opcom_version), "EMU0");
    (void)snprintf(tp->compile_time, sizeof(tp->compile_time), "nd500x emulated    ");
}

/*
 * Build the reply payload for one command. Returns the payload byte count, or 0
 * for "stay silent" (a malformed request, or a reply that does not fit).
 */
static int build_reply(NdbusTestProto *tp, const NdbusStation *station, uint16_t command,
                       const uint8_t *message, int length, uint8_t *payload, int payload_max)
{
    const uint8_t number = station->number;

    switch (command)
    {
    case NDBUS_TESTPROTO_CMD_IDENTIFY:
        /* Header only, 8 bytes. TPE test 4 reads nothing past the header and
         * requires EXACTLY ONE reply (NDBusOctobus.cs line 441). */
        return build_header(payload, payload_max, number, command, NDBUS_TESTPROTO_STATUS_OK, 0);

    case NDBUS_TESTPROTO_CMD_GET_PRESENT:
    {
        /* 66 words = 132 bytes: the header, then one word per station j = 1..62
         * at word 3 + j, value exactly 1 when that station is present
         * (NDBusOctobus.cs line 443-458). COMPUTED from the live fabric registry:
         * TPE compares each word against its own presence bitmap, so a canned
         * answer would disagree with whatever the machine is configured with. */
        int bytes = build_header(payload, payload_max, number, command,
                                 NDBUS_TESTPROTO_STATUS_OK, 62);
        if (bytes == 0)
        {
            return 0;
        }
        for (uint8_t j = 1u; j <= NDBUS_STATION_MAX; j++)
        {
            const bool present = ndbus_fabric_has_station(station->fabric, j);
            put_word(payload, 3 + (int)j, present ? (uint16_t)1u : (uint16_t)0u);
        }
        return bytes;
    }

    case NDBUS_TESTPROTO_CMD_ECHO_SINGLE:
    {
        /* Words 4 and 5 echo request words 2 and 3 - the pattern number and the
         * pattern - for 12 bytes total (NDBusOctobus.cs line 460-472). TPE
         * compares BOTH, so a reply that echoes only the pattern fails. */
        uint16_t pattern_number = 0;
        uint16_t pattern        = 0;
        if (!request_word(message, length, 2, &pattern_number) ||
            !request_word(message, length, 3, &pattern))
        {
            return 0;
        }
        int bytes =
            build_header(payload, payload_max, number, command, NDBUS_TESTPROTO_STATUS_OK, 2);
        if (bytes == 0)
        {
            return 0;
        }
        put_word(payload, 4, pattern_number);
        put_word(payload, 5, pattern);
        return bytes;
    }

    case NDBUS_TESTPROTO_CMD_ECHO_MULTI:
    {
        /* Request words 2 = string number, 3 = word count N, 4..3+N = the string.
         * The reply echoes all of it at words 4, 5 and 6..5+N, so 12 + 2N bytes
         * (NDBusOctobus.cs line 474-500). TPE compares every word.
         *
         * TPE caps its request at 250 payload bytes ([cfcc] in the C# citation),
         * so a conforming N is at most 121. A larger N, or one the request does
         * not actually carry, is refused with silence rather than padded. */
        uint16_t string_number = 0;
        uint16_t word_count    = 0;
        if (!request_word(message, length, 2, &string_number) ||
            !request_word(message, length, 3, &word_count))
        {
            return 0;
        }
        if (word_count > 121u)
        {
            return 0;
        }
        int bytes = build_header(payload, payload_max, number, command,
                                 NDBUS_TESTPROTO_STATUS_OK, 2 + (int)word_count);
        if (bytes == 0)
        {
            return 0;
        }
        put_word(payload, 4, string_number);
        put_word(payload, 5, word_count);
        for (int k = 0; k < (int)word_count; k++)
        {
            uint16_t string_word = 0;
            if (!request_word(message, length, 4 + k, &string_word))
            {
                return 0;
            }
            put_word(payload, 6 + k, string_word);
        }
        return bytes;
    }

    case NDBUS_TESTPROTO_CMD_READ_REGISTER:
    {
        /* Request word 2 = register function; TPE prompts "Register function
         * (0,2,6)." Reply word 4 = the register content, 10 bytes
         * (NDBusOctobus.cs line 502-520).
         *
         * UNVERIFIED: what those function codes actually select on a real station
         * is not recorded anywhere available, and the C# reference says the same.
         * This serves a plain register file so the interactive TPE commands get a
         * protocol-correct, stateful answer, and refuses any other function with
         * status 1, which is the protocol's own reject. */
        uint16_t function = 0;
        if (!request_word(message, length, 2, &function))
        {
            return 0;
        }
        const bool legal = (function == 0u) || (function == 2u) || (function == 6u);
        int        bytes = build_header(payload, payload_max, number, command,
                                        legal ? NDBUS_TESTPROTO_STATUS_OK
                                              : NDBUS_TESTPROTO_STATUS_BAD_REGISTER_FN,
                                        1);
        if (bytes == 0)
        {
            return 0;
        }
        put_word(payload, 4, legal ? tp->registers[function] : (uint16_t)0u);
        return bytes;
    }

    case NDBUS_TESTPROTO_CMD_WRITE_REGISTER:
    {
        /* Request word 2 = register function ("(3,5,7)."), word 3 = the content.
         * The reply is a header-only acknowledge, 8 bytes (NDBusOctobus.cs line
         * 522-536). Same UNVERIFIED function semantics as the read. */
        uint16_t function = 0;
        uint16_t content  = 0;
        if (!request_word(message, length, 2, &function) ||
            !request_word(message, length, 3, &content))
        {
            return 0;
        }
        const bool legal = (function == 3u) || (function == 5u) || (function == 7u);
        if (legal)
        {
            tp->registers[function] = content;
        }
        return build_header(payload, payload_max, number, command,
                            legal ? NDBUS_TESTPROTO_STATUS_OK
                                  : NDBUS_TESTPROTO_STATUS_BAD_REGISTER_FN,
                            0);
    }

    case NDBUS_TESTPROTO_CMD_GET_DOMINO:
    {
        /* 36 bytes: words 4-5 = processor type as one 32-bit number, 6-7 = OPCOM
         * version (4 ASCII bytes), 8-17 = compile time (20 ASCII bytes)
         * (NDBusOctobus.cs line 538-548). The VALUES are UNVERIFIED placeholders -
         * see ndbus_testproto_init. */
        int bytes =
            build_header(payload, payload_max, number, command, NDBUS_TESTPROTO_STATUS_OK, 14);
        if (bytes == 0)
        {
            return 0;
        }
        put_word(payload, 4, (uint16_t)(tp->processor_type >> 16u));
        put_word(payload, 5, (uint16_t)(tp->processor_type & 0xFFFFu));
        put_ascii(payload, 6, tp->opcom_version, NDBUS_TESTPROTO_OPCOM_BYTES);
        put_ascii(payload, 8, tp->compile_time, NDBUS_TESTPROTO_COMPILE_BYTES);
        return bytes;
    }

    case NDBUS_TESTPROTO_CMD_GET_VERSION:
    {
        /* Word 4 = the octobus test version, 10 bytes (NDBusOctobus.cs line
         * 550-554). The VALUE is UNVERIFIED. */
        int bytes =
            build_header(payload, payload_max, number, command, NDBUS_TESTPROTO_STATUS_OK, 1);
        if (bytes == 0)
        {
            return 0;
        }
        put_word(payload, 4, tp->test_version);
        return bytes;
    }

    case NDBUS_TESTPROTO_CMD_GET_MODULE:
    {
        /* Word 4 = the module type, 10 bytes (NDBusOctobus.cs line 556-563).
         * OctobusND5000Station.cs line 702 sets it to 3 = ACCP: the ND-5000
         * station IS the ACCP baby card. */
        int bytes =
            build_header(payload, payload_max, number, command, NDBUS_TESTPROTO_STATUS_OK, 1);
        if (bytes == 0)
        {
            return 0;
        }
        put_word(payload, 4, tp->module_type);
        return bytes;
    }

    default:
        /* Header only, status 2 "Illegal Test Protocol command code" - the
         * protocol's own answer for a message we do not understand
         * (NDBusOctobus.cs line 565-569). NOT silence: silence means "no station
         * there", and this station is very much there. */
        return build_header(payload, payload_max, number, command,
                            NDBUS_TESTPROTO_STATUS_BAD_COMMAND, 0);
    }
}

int ndbus_testproto_answer(NdbusTestProto *tp, const NdbusStation *station,
                           const uint8_t *message, int length, uint16_t *replies, int max)
{
    if (tp == NULL || station == NULL || message == NULL || replies == NULL)
    {
        return 0;
    }

    /* Source OMD, byte count, magic word, command word: below six bytes there is
     * no command to run (NDBusOctobus.cs line 377). */
    if (length < 6)
    {
        return 0;
    }

    const uint8_t reply_omd = (uint8_t)(message[0] & 0x0Fu);

    /* The payload must start with the magic. TPE checks the same on OUR replies
     * (octobus_parse_received_frame @ ram:d080 per the C# citation), and a
     * message without it is not a Test Protocol message at all - so it is not
     * answered, rather than answered with an error. */
    if (message[2] != 0x71u || message[3] != 0xC7u)
    {
        return 0;
    }

    const uint16_t command = (uint16_t)(((uint16_t)message[4] << 8) | (uint16_t)message[5]);
    tp->messages++;
    tp->last_command = command;

    /* The longest reply is echo-multi with N = 121: (4 + 2 + 121) words = 254
     * bytes, which still fits the one-byte wire count. */
    uint8_t payload[NDBUS_MULTIBYTE_MAX];
    int     bytes = build_reply(tp, station, command, message, length, payload,
                                (int)sizeof(payload));
    if (bytes == 0)
    {
        return 0;
    }

    /*
     * The reply goes back to the OMD the sender named in byte 0, and OUR source
     * OMD is 0: we answer as the test-protocol module (NDBusOctobus.cs line 416).
     */
    int frames = ndbus_multibyte_build(station->number, reply_omd, (uint8_t)NDBUS_TESTPROTO_OMD,
                                       payload, bytes, replies, max);
    if (frames > 0)
    {
        tp->replies++;
    }
    return frames;
}
