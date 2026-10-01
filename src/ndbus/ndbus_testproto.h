/**
 * @file ndbus_testproto.h
 * @brief The OMD-0 Octobus Test Protocol responder.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * WHAT THIS IS, AND WHY IT IS NOT THE ACCP PROTOCOL.
 *
 * ND-05.020.01 ~3930 reserves OMD 0 for the octobus test programs. It is a
 * SECOND, INDEPENDENT multibyte protocol, served by the station firmware itself -
 * on the ND-5000 that is the ACCP, never the microprogram, which has no multibyte
 * driver at all. The ACCP command library of ndbus_accp.h rides on OMD 3 and
 * answers Messack/Messnak; nothing of that applies here. The two paths share only
 * the multibyte envelope in ndbus_multibyte.h.
 *
 * WHO ASKS. The ND diagnostic TPE OCTOBUS B00. Its test 4 "Check Octobus
 * configuration" scans every station that acknowledges frames and asks it to
 * identify itself; tests 5 and 6 echo patterns against the stations that
 * answered. A station that acknowledges frames but stays mute on OMD 0 is
 * reported as absent, which is exactly what a live run produced for the ND-5000
 * at 070B before this file existed ("No Domino controllers are present. Test
 * aborted.").
 *
 * WIRE LAYOUT of a collected OMD-0 message - the same envelope every multibyte
 * message has (ND-05.020.01 T124 figure 28):
 *
 *     byte 0    sender's OMD number: the reply is addressed BACK to this OMD
 *     byte 1    payload byte count
 *     byte 2..  payload: magic word 0x71C7, command word, command parameters
 *
 * All payload words are big-endian (high byte first).
 *
 * PROVENANCE. Ported from RetroCore Emulated.HW/ND/CPU/NDBUS/NDBusOctobus.cs,
 * members AnswerTestProtocolMessage (line 367), BuildTestProtocolReply (line 434)
 * and BuildTestProtocolHeader (line 613), whose own comments cite the TPE
 * OCTOBUS B00 disassembly (magic checked in octobus_parse_received_frame @
 * ram:d080, reply words stored high-byte-first at ram:d35d-d36a, status codes
 * decoded @ ram:8f50). The recorded request bodies this responder is tested
 * against come from RetroCore
 * Emulated.Tests.ND100/ControllerOctobus/OctobusTpeConfigReproTests.cs.
 *
 * NOT VERIFIED HERE. The TPE disassembly itself was not read while writing this
 * file; the reply layouts are the C# reference's, and the per-field doubts the
 * reference records are carried over verbatim below rather than resolved. Every
 * value marked UNVERIFIED would be settled by one live capture of a real DOMINO
 * station answering that command.
 */

#ifndef NDBUS_TESTPROTO_H
#define NDBUS_TESTPROTO_H

#include "ndbus_octobus.h"

/** @brief The OMD number the Test Protocol lives on (ND-05.020.01 ~3930). */
#define NDBUS_TESTPROTO_OMD 0u

/** @brief First payload word of every request and every reply. */
#define NDBUS_TESTPROTO_MAGIC ((uint16_t)0x71C7u)

/* ---- commands (request payload word 1) ------------------------------------- */

/** @brief Identify yourself: reply is the 4-word header and nothing else. */
#define NDBUS_TESTPROTO_CMD_IDENTIFY       ((uint16_t)0x0000u)
/** @brief Get present stations: reply carries one word per station 1..62. */
#define NDBUS_TESTPROTO_CMD_GET_PRESENT    ((uint16_t)0x000Au)
/** @brief Echo single word: reply echoes the pattern number and the pattern. */
#define NDBUS_TESTPROTO_CMD_ECHO_SINGLE    ((uint16_t)0x000Cu)
/** @brief Echo multi word: reply echoes a string number, a count and the string. */
#define NDBUS_TESTPROTO_CMD_ECHO_MULTI     ((uint16_t)0x000Eu)
/** @brief Read octobus register: reply carries the register content. */
#define NDBUS_TESTPROTO_CMD_READ_REGISTER  ((uint16_t)0x0010u)
/** @brief Write octobus register: reply is a header-only acknowledge. */
#define NDBUS_TESTPROTO_CMD_WRITE_REGISTER ((uint16_t)0x0012u)
/** @brief Get Domino information: processor type, OPCOM version, compile time. */
#define NDBUS_TESTPROTO_CMD_GET_DOMINO     ((uint16_t)0x0016u)
/** @brief Get test version: reply carries the octobus test version number. */
#define NDBUS_TESTPROTO_CMD_GET_VERSION    ((uint16_t)0x0018u)
/** @brief Get module type: reply carries one of the NDBUS_TESTPROTO_MODULE_ codes. */
#define NDBUS_TESTPROTO_CMD_GET_MODULE     ((uint16_t)0x001Au)

/* ---- status (reply header word 3) ------------------------------------------ */

/** @brief Status 0: the command was carried out. */
#define NDBUS_TESTPROTO_STATUS_OK               ((uint16_t)0u)
/** @brief Status 1: illegal octobus register function. */
#define NDBUS_TESTPROTO_STATUS_BAD_REGISTER_FN  ((uint16_t)1u)
/** @brief Status 2: illegal Test Protocol command code. */
#define NDBUS_TESTPROTO_STATUS_BAD_COMMAND      ((uint16_t)2u)

/* ---- module type codes (Get module type, reply word 4) --------------------- */

/** @brief Module type 1: Domino controller. */
#define NDBUS_TESTPROTO_MODULE_DOMINO ((uint16_t)1u)
/** @brief Module type 2: MFbus controller. */
#define NDBUS_TESTPROTO_MODULE_MFBUS  ((uint16_t)2u)
/** @brief Module type 3: ACCP - what an ND-5000 station is. */
#define NDBUS_TESTPROTO_MODULE_ACCP   ((uint16_t)3u)

/** @brief Room for the register file served by the read/write register commands. */
#define NDBUS_TESTPROTO_REGISTERS 8u

/** @brief Fixed bytes of the Get Domino information OPCOM version field. */
#define NDBUS_TESTPROTO_OPCOM_BYTES 4
/** @brief Fixed bytes of the Get Domino information compile-time field. */
#define NDBUS_TESTPROTO_COMPILE_BYTES 20

/**
 * @brief What this station answers with, and the register file it keeps.
 *
 * Every field except `module_type` is UNVERIFIED: nothing available records what
 * a real DOMINO station puts in the Get Domino information, Get test version or
 * register replies, and the C# reference says so too (NDBusOctobus.cs lines
 * 578-607). They are emulator placeholders that make the interactive TPE
 * commands answer in the right SHAPE. `module_type` is the one field with a
 * documented meaning: 1 Domino controller, 2 MFbus controller, 3 ACCP.
 */
typedef struct NdbusTestProto
{
    uint32_t processor_type;  /**< 32-bit "type of processor"; UNVERIFIED */
    uint16_t test_version;    /**< octobus test version; UNVERIFIED */
    uint16_t module_type;     /**< NDBUS_TESTPROTO_MODULE_*; ND-5000 is ACCP */
    char     opcom_version[NDBUS_TESTPROTO_OPCOM_BYTES + 1];   /**< ASCII; UNVERIFIED */
    char     compile_time[NDBUS_TESTPROTO_COMPILE_BYTES + 1];  /**< ASCII; UNVERIFIED */
    uint16_t registers[NDBUS_TESTPROTO_REGISTERS]; /**< served by read/write register */

    /* Diagnostics, so a test can say what happened rather than infer it. */
    unsigned long messages;      /**< complete OMD-0 messages accepted */
    unsigned long replies;       /**< replies actually sent */
    uint16_t      last_command;  /**< command word of the last message parsed */
} NdbusTestProto;

/**
 * @brief Bring a responder up with the placeholder answers above.
 * @param tp          The responder state.
 * @param module_type The NDBUS_TESTPROTO_MODULE_ code this station reports.
 * @return Nothing. Cannot fail.
 */
void ndbus_testproto_init(NdbusTestProto *tp, uint16_t module_type);

/**
 * @brief Answer one complete OMD-0 multibyte message.
 *
 * @param tp      The responder state.
 * @param station The station answering. Its `number` goes in reply header word 2
 *                and in bits 13-8 of every reply frame, and its `fabric` is the
 *                registry the Get present stations reply is computed from - with
 *                no fabric that reply reports every station absent.
 * @param message The collected message body: source OMD, byte count, payload.
 * @param length  How many body bytes `message` holds.
 * @param replies Buffer for the reply frames.
 * @param max     Room in `replies`, in frames.
 * @return The number of reply frames written, or 0 for "stay silent". Silence is
 *         what the protocol has instead of an error return: a message shorter
 *         than its own header, a payload without the 0x71C7 magic, a malformed
 *         command and a reply that does not fit all produce no frames, and the
 *         asker sees a receive timeout. An UNKNOWN command is NOT silence - it
 *         gets a header-only reply with status 2.
 */
int ndbus_testproto_answer(NdbusTestProto *tp, const NdbusStation *station,
                           const uint8_t *message, int length, uint16_t *replies, int max);

#endif /* NDBUS_TESTPROTO_H */
