/**
 * @file ndbus_accp.h
 * @brief The ACCP command layer: codes, guards and the Messack/Messnak reply.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * The Access Module (ACCP) is the ND-5000's octobus front end. The ND-120 talks
 * to it to debug, test, initialize and monitor the ND-5000 - loading the control
 * store, starting and stopping the microprogram, asking whether the CPU is
 * alive.
 *
 * TWO SOURCES, AND THEY DISAGREE. READ THIS BEFORE CHANGING A NUMBER.
 *
 * 1. THE MANUAL gives the message FRAME, the reply protocol and the error-code
 *    table. ND-05.020.01 chapter 5, transcription markers T123 (5.3.10, ACCP
 *    Command Structure), T124 (5.3.11, ACCP Command Specifications) and T125.
 *
 *    T124: "All commands are activated by a multibyte message from the ND-120
 *    over the octobus. ... The command name and parameters, as described here,
 *    make up the body of the multibyte message, and the message frame is
 *    added/removed by the octobus driver. The command itself is specified in the
 *    first byte in the message body." Destination OMD = 3 for ACCP device
 *    handling. "The parameter field is organized in 16-bit words, since both the
 *    ND-120 and the ACCP are 16-bit processors. Direct parameters with several
 *    bytes always have the most significant byte first."
 *
 *    T125, the reply: "In response to a multibyte message, the ACCP sends either
 *    Message acknowledged (Messack) or Message not acknowledged (Messnak). The
 *    first (Messack) is only a single byte which is used if everything went OK.
 *    The second (Messnak) returns an error code and two bytes of status".
 *    Byte 0 is the command error code, bytes 1 and 2 are the hardware status
 *    register ASTS, lower then upper.
 *
 * 2. THE NUMERIC COMMAND BYTES ARE NOT IN THE MANUAL. The manual numbers its
 *    SECTIONS (5.3.12 Echo Test, 5.3.23 Start Microprogram) and never states a
 *    command byte. The values below come from a carve of the REAL ND-324716 ACCP
 *    firmware and from a measured state matrix - RetroCore's
 *    `$RETROCORE/Emulated.HW/ND/CPU/NDBUS/AccpCommandGuards.cs`, whose header
 *    cites the ACCP-OCTOBUS-COMMAND-TABLE carve and the raw sweep output kept in
 *    the ND5000UC repository. Its own words: "Nothing here is taken from the
 *    manual alone."
 *
 *    AND THE MANUAL'S ORDER IS NOT THE COMMAND ORDER. RetroCore records that
 *    033B is RUNSELFT, not STARTMIC, and that the real start-microprogram
 *    command is 066B - 0x36. Deriving a command byte from a section number
 *    produces a table that looks right and starts nothing.
 *
 * So: the FRAME and the ERROR CODES are manual-verified; the COMMAND BYTES and
 * the GUARD RULES are firmware-measured. Never fill a gap in one from the other.
 */

#ifndef NDBUS_ACCP_H
#define NDBUS_ACCP_H

#include "ndbus_types.h"

/* ---- command codes ----------------------------------------------------------
 *
 * Firmware-measured (see the header comment). The octal spelling in each comment
 * is the one the firmware carve uses; the C value is hexadecimal, because an ND
 * octal literal written as a bare decimal is the mistake that produced
 * "ND5000_CPU = 70" in RetroCore's station enum.
 *
 * The dispatcher has an arm for 0x0D..0x3E with four holes: 0x19, 0x1A, 0x2E and
 * 0x2F. Anything else naks 6, "not defined as ACCP command".
 */

/** @brief 015B: read system parameters back. */
#define NDBUS_ACCP_RSSYSPAR   0x0Du  /* 015B  read system parameters back */
/** @brief 016B: load system parameters (6 bytes). */
#define NDBUS_ACCP_LSYSPAR    0x0Eu  /* 016B  load system parameters (6 bytes) */
/** @brief 017B: echo test. */
#define NDBUS_ACCP_ECHO       0x0Fu  /* 017B  echo test */
/** @brief 020B: read ECO levels. */
#define NDBUS_ACCP_RECO       0x10u  /* 020B  read ECO levels */
/** @brief 021B: load parameter pointer (4 bytes). */
#define NDBUS_ACCP_LPARP      0x11u  /* 021B  load parameter pointer (4 bytes) */
/** @brief 022B: verify parameter pointer. */
#define NDBUS_ACCP_VPARP      0x12u  /* 022B  verify parameter pointer */
/** @brief 023B: load control store via memory. */
#define NDBUS_ACCP_LOCSM      0x13u  /* 023B  load control store via memory */
/** @brief 024B: load control store directly. */
#define NDBUS_ACCP_LOCSD      0x14u  /* 024B  load control store directly */
/** @brief 025B: dump control store via memory. */
#define NDBUS_ACCP_DUCS       0x15u  /* 025B  dump control store via memory */
/** @brief 026B: dump control store directly. */
#define NDBUS_ACCP_DCSD       0x16u  /* 026B  dump control store directly */
/** @brief 033B: run self-test. NOT StartMic - see the header comment. */
#define NDBUS_ACCP_RUNTST     0x1Bu  /* 033B  run self-test. NOT StartMic. */
/** @brief 034B: stop microprogram. */
#define NDBUS_ACCP_STOPMIC    0x1Cu  /* 034B  stop microprogram */
/** @brief 035B: continue microprogram. */
#define NDBUS_ACCP_CONTMIC    0x1Du  /* 035B  continue microprogram */
/** @brief 036B: restart microprogram. */
#define NDBUS_ACCP_RESTMIC    0x1Eu  /* 036B  restart microprogram */
/** @brief 037B: alive check. */
#define NDBUS_ACCP_ALIVE      0x1Fu  /* 037B  alive check */
/** @brief 040B: load MAR. */
#define NDBUS_ACCP_LMAR       0x20u  /* 040B  load MAR */
/** @brief 041B: load MIR (128-bit). */
#define NDBUS_ACCP_LMIR       0x21u  /* 041B  load MIR (128-bit) */
/** @brief 042B: read MIR. */
#define NDBUS_ACCP_RMIR       0x22u  /* 042B  read MIR */
/** @brief 043B: test buffer. */
#define NDBUS_ACCP_TBUF       0x23u  /* 043B  test buffer */
/** @brief 044B: read AIB16. */
#define NDBUS_ACCP_RAIB16     0x24u  /* 044B  read AIB16 */
/** @brief 045B: read AIB32 directly. */
#define NDBUS_ACCP_RAIB32D    0x25u  /* 045B  read AIB32 directly */
/** @brief 046B: test bus. */
#define NDBUS_ACCP_TBUS       0x26u  /* 046B  test bus */
/** @brief 047B: load AOB16. */
#define NDBUS_ACCP_LAOB16     0x27u  /* 047B  load AOB16 */
/** @brief 051B: load mode. */
#define NDBUS_ACCP_LMODE      0x29u  /* 051B  load mode */
/** @brief 052B: load CON. */
#define NDBUS_ACCP_LCON       0x2Au  /* 052B  load CON */
/** @brief 053B: write multiport. */
#define NDBUS_ACCP_WMPM       0x2Bu  /* 053B  write multiport */
/** @brief 054B: read multiport. */
#define NDBUS_ACCP_RMPM       0x2Cu  /* 054B  read multiport */
/** @brief 055B: set trace selector. */
#define NDBUS_ACCP_SETTRAC    0x2Du  /* 055B  set trace selector */
/** @brief 060B: read self-test status. */
#define NDBUS_ACCP_READSELFT  0x30u  /* 060B  read self-test status */
/** @brief 061B: enable kicks. */
#define NDBUS_ACCP_ENKICK     0x31u  /* 061B  enable kicks */
/** @brief 062B: disable kicks. */
#define NDBUS_ACCP_DISKICK    0x32u  /* 062B  disable kicks */
/** @brief 063B: load AOB32 directly. */
#define NDBUS_ACCP_LAOB32D    0x33u  /* 063B  load AOB32 directly */
/** @brief 064B: load AOB32 via memory. */
#define NDBUS_ACCP_LAOB32M    0x34u  /* 064B  load AOB32 via memory */
/** @brief 065B: read AIB32 via memory. */
#define NDBUS_ACCP_RAIB32M    0x35u  /* 065B  read AIB32 via memory */
/** @brief 066B: START MICROPROGRAM - the real one, not 033B. */
#define NDBUS_ACCP_STARTMIC   0x36u  /* 066B  START MICROPROGRAM - the real one */
/** @brief 070B: ACCP microtrap. */
#define NDBUS_ACCP_AMICTRAP   0x38u  /* 070B  ACCP microtrap */
/** @brief 071B: reset the ND-5000 CPU. */
#define NDBUS_ACCP_CPURES     0x39u  /* 071B  reset the ND-5000 CPU */
/** @brief 072B: test multiport. */
#define NDBUS_ACCP_TESTMPM    0x3Au  /* 072B  test multiport */
/** @brief 073B: dump control cache directly. */
#define NDBUS_ACCP_DCCD       0x3Bu  /* 073B  dump control cache directly */
/** @brief 074B: dump control cache via memory. */
#define NDBUS_ACCP_DUCC       0x3Cu  /* 074B  dump control cache via memory */
/** @brief 075B: read ACCP PROM version. */
#define NDBUS_ACCP_PRGMVERS   0x3Du  /* 075B  read ACCP PROM version */
/** @brief 076B: read CPU model. */
#define NDBUS_ACCP_CPUMODEL   0x3Eu  /* 076B  read CPU model */

/** @brief Destination OMD for ACCP device handling (T124, figure 28). */
#define NDBUS_ACCP_OMD 3u

/* ---- Messnak error codes -----------------------------------------------------
 *
 * MANUAL-VERIFIED, ND-05.020.01 T125, "Messnak error codes". Sent on the wire as
 * a byte, so -1 is 0xFF and -2 is 0xFE.
 */

/** @brief -2: illegal when kicks are enabled. */
#define NDBUS_ACCP_NAK_KICKS_ENABLED       (-2) /* illegal when kicks are enabled */
/** @brief -1: illegal when the microprogram is running. */
#define NDBUS_ACCP_NAK_MICRO_RUNNING       (-1) /* illegal when microprogram is running */
/** @brief 0: the microprogram is not started. */
#define NDBUS_ACCP_NAK_MICRO_NOT_STARTED     0  /* microprogram is not started */
/** @brief 1: no parameter pointer is given. */
#define NDBUS_ACCP_NAK_NO_PARAM_POINTER      1  /* no parameter pointer is given */
/** @brief 2: illegal word count. */
#define NDBUS_ACCP_NAK_ILLEGAL_WORD_COUNT    2
/** @brief 3: illegal address. */
#define NDBUS_ACCP_NAK_ILLEGAL_ADDRESS       3
/** @brief 4: checksum error. */
#define NDBUS_ACCP_NAK_CHECKSUM              4
/** @brief 5: control store hardware error. */
#define NDBUS_ACCP_NAK_CS_HW_ERROR           5
/** @brief 6: not defined as an ACCP command. */
#define NDBUS_ACCP_NAK_UNDEFINED_COMMAND     6
/** @brief 7: not alive. */
#define NDBUS_ACCP_NAK_NOT_ALIVE             7

/* The two ACCP hardware-status (ASTS) bytes a long-form Messnak carries. Measured on
 * the real ND-324716 firmware, 2026-09-18, where they read 10 11 while the
 * microprogram is not running. RetroCore OctobusND5000Station.cs:3678 and :3681. */
#define NDBUS_ACCP_ASTS_HIGH              0x10u
#define NDBUS_ACCP_ASTS_LOW               0x11u
/** @brief 8: memory error. */
#define NDBUS_ACCP_NAK_MEMORY_ERROR          8
/** @brief 9: control store not initialized. */
#define NDBUS_ACCP_NAK_CS_NOT_INITIALIZED    9

/**
 * @brief 13: system parameters have not been given.
 *
 * FIRMWARE-MEASURED AND NOT IN THE MANUAL: arm 0x0D (read system parameters
 * back) naks 13 until LSYSPAR has been given, and that nak is the SHORT two-byte
 * form with no status bytes.
 */
#define NDBUS_ACCP_NAK_SYSPAR_NOT_GIVEN     13

/**
 * @brief Returned by ndbus_accp_evaluate() when the command is accepted.
 *
 * Chosen outside the -2..13 range of real codes so no nak can collide with it.
 */
#define NDBUS_ACCP_ACCEPTED               1000

/**
 * @brief The guard state, as the real ND-324716 firmware keeps it in RAM.
 *
 * Each field names the firmware cell it stands for, from the
 * ACCP-OCTOBUS-COMMAND-TABLE carve.
 */
typedef struct NdbusAccpState
{
    bool microprogram_running;  /**< cell 0x1143AC: started and not stopped */
    bool system_parameters_given; /**< cell 0x1143A6: LSYSPAR 0x0E received */
    bool parameter_pointer_given; /**< cell 0x1143B2: LPARP 0x11 received */
    bool kicks_enabled;         /**< cell 0x1143B6: ENKICK 0x31 in force */
} NdbusAccpState;

/**
 * @brief Whether the firmware's compare chain has an arm for this command code.
 * @param command The command byte from the first byte of the message body.
 * @return true for 0x0D..0x3E except the four holes 0x19, 0x1A, 0x2E and 0x2F;
 *         false otherwise, which is the case the firmware naks 6 for.
 */
bool ndbus_accp_has_arm(uint8_t command);

/**
 * @brief Decide whether a command is accepted in the current guard state.
 *
 * THE GUARD ORDER IS MEASURED, NOT GUESSED: matrix state S5 - running with no
 * pointer given - answers -1 for 0x12/0x13/0x15/0x3C and 1 for 0x34/0x35, so the
 * running guard is tested before the parameter-pointer guard.
 *
 * @param command The command byte.
 * @param state   The current ACCP guard state.
 * @return NDBUS_ACCP_ACCEPTED (1000) when the command is accepted, otherwise the
 *         Messnak code the real firmware answers, which is one of the
 *         NDBUS_ACCP_NAK_* values in the range -2..13. The accepted value is
 *         outside that range on purpose, so no nak can be mistaken for success.
 */
int ndbus_accp_evaluate(uint8_t command, const NdbusAccpState *state);

/**
 * @brief Whether a Messnak for this code is the short two-byte form.
 * @param nak_code A Messnak code as returned by ndbus_accp_evaluate().
 * @return true when the reply carries no ASTS status bytes. Measured for code 13
 *         only (arm 0x0D), as the two bytes FF 0D; false for every other code,
 *         including codes that are not naks at all.
 */
bool ndbus_accp_nak_is_short(int nak_code);

/**
 * @brief How many parameter bytes the real firmware needs before it answers.
 *
 * A message with fewer gets NO REPLY AT ALL and changes nothing - which is not
 * the same as a nak, and a caller that treats silence as an error will report a
 * fault the real card does not report.
 *
 * @param command The command byte.
 * @return The minimum number of parameter bytes. 0 means the command takes none,
 *         or its length was not measured - the two cases are not distinguished,
 *         and 0 is never an error indication. Deliberately 0 for LOCSM 0x13,
 *         DUCS 0x15, LAOB32M 0x34, RAIB32M 0x35 and DUCC 0x3C: they take their
 *         parameters from MFbus memory at the LPARP pointer, and SINTRAN sends
 *         them bare.
 */
int ndbus_accp_min_parameter_bytes(uint8_t command);

/**
 * @brief Whether this command's firmware arm reads parameters before its guards.
 *
 * True for the commands whose firmware arm reads its parameters BEFORE testing
 * the state guards, so a too-short message gets no reply even in a state where
 * the full message would be refused. Measured: LOCSD 0x14 and DCSD 0x16 only -
 * every other measured command tests its guards first, and a bare STARTMIC 0x36
 * while running still naks -1.
 *
 * @param command The command byte.
 * @return true for LOCSD 0x14 and DCSD 0x16, false for every other command.
 */
bool ndbus_accp_length_checked_first(uint8_t command);

/**
 * @brief A short human name for logs and test failures.
 * @param command The command byte.
 * @return The command's name. Never NULL: an unknown command yields a
 *         placeholder string rather than a null pointer, so a log line can never
 *         fault on it.
 */
const char *ndbus_accp_command_name(uint8_t command);

#endif /* NDBUS_ACCP_H */
