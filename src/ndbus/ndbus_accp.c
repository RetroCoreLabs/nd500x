/*
 * ndbus_accp.c - the ACCP command layer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * Every rule here is firmware-measured, not read out of the manual. See the
 * header for which half comes from where.
 */

#include "ndbus_accp.h"

bool ndbus_accp_has_arm(uint8_t command)
{
    if (command < 0x0Du || command > 0x3Eu)
    {
        return false;
    }
    /* The four holes inside the range. */
    return command != 0x19u && command != 0x1Au && command != 0x2Eu && command != 0x2Fu;
}

static bool illegal_while_running(uint8_t command)
{
    switch (command)
    {
    case NDBUS_ACCP_VPARP:
    case NDBUS_ACCP_LOCSM:
    case NDBUS_ACCP_LOCSD:
    case NDBUS_ACCP_DUCS:
    case NDBUS_ACCP_DCSD:
    case NDBUS_ACCP_RUNTST:
    case NDBUS_ACCP_RESTMIC:
    case NDBUS_ACCP_RMIR:
    case NDBUS_ACCP_RAIB16:
    case NDBUS_ACCP_STARTMIC:
    case NDBUS_ACCP_DCCD:
    case NDBUS_ACCP_DUCC:
        return true;
    default:
        return false;
    }
}

static bool illegal_while_kicks_enabled(uint8_t command)
{
    /* Reading the AIB would destroy a kick being sent from the ND-5000. */
    return command == NDBUS_ACCP_RAIB32D || command == NDBUS_ACCP_LAOB32M ||
           command == NDBUS_ACCP_RAIB32M;
}

static bool needs_parameter_pointer(uint8_t command)
{
    switch (command)
    {
    case NDBUS_ACCP_VPARP:
    case NDBUS_ACCP_LOCSM:
    case NDBUS_ACCP_DUCS:
    case NDBUS_ACCP_LAOB32M:
    case NDBUS_ACCP_RAIB32M:
    case NDBUS_ACCP_DUCC:
        return true;
    default:
        return false;
    }
}

int ndbus_accp_evaluate(uint8_t command, const NdbusAccpState *state)
{
    if (state == NULL)
    {
        return NDBUS_ACCP_NAK_UNDEFINED_COMMAND;
    }

    if (!ndbus_accp_has_arm(command))
    {
        return NDBUS_ACCP_NAK_UNDEFINED_COMMAND;
    }

    /* STOPMIC has the INVERSE guard: it is the one command that is refused when
     * the microprogram is NOT running. */
    if (command == NDBUS_ACCP_STOPMIC)
    {
        return state->microprogram_running ? NDBUS_ACCP_ACCEPTED
                                           : NDBUS_ACCP_NAK_MICRO_NOT_STARTED;
    }

    /* Reading the system parameters back before they were loaded. The nak code
     * and its short form are both firmware-measured; neither is in the manual. */
    if (command == NDBUS_ACCP_RSSYSPAR)
    {
        return state->system_parameters_given ? NDBUS_ACCP_ACCEPTED
                                              : NDBUS_ACCP_NAK_SYSPAR_NOT_GIVEN;
    }

    /* ORDER IS MEASURED. Running is tested before the parameter pointer: in
     * matrix state S5 - running, no pointer given - 0x12/0x13/0x15/0x3C answer
     * -1 and only 0x34/0x35 answer 1. Swapping these two blocks would change
     * four of those answers. */
    if (state->microprogram_running && illegal_while_running(command))
    {
        return NDBUS_ACCP_NAK_MICRO_RUNNING;
    }

    if (state->kicks_enabled && illegal_while_kicks_enabled(command))
    {
        return NDBUS_ACCP_NAK_KICKS_ENABLED;
    }

    if (!state->parameter_pointer_given && needs_parameter_pointer(command))
    {
        return NDBUS_ACCP_NAK_NO_PARAM_POINTER;
    }

    /*
     * ALIVE 0x1F is deliberately NOT decided here. The real card naks 7 ("not
     * alive") from a hardware alive signal, not from the running cell, so a
     * guard table cannot answer it - the station does, from whether its CPU is
     * actually attached.
     */
    return NDBUS_ACCP_ACCEPTED;
}

bool ndbus_accp_nak_is_short(int nak_code)
{
    return nak_code == NDBUS_ACCP_NAK_SYSPAR_NOT_GIVEN;
}

int ndbus_accp_min_parameter_bytes(uint8_t command)
{
    switch (command)
    {
    case NDBUS_ACCP_ECHO:      /* the count byte; the echoed bytes are not checked here */
    case NDBUS_ACCP_AMICTRAP:
        return 1;
    case NDBUS_ACCP_DCSD:
    case NDBUS_ACCP_LMAR:
    case NDBUS_ACCP_TBUS:
    case NDBUS_ACCP_LMODE:
    case NDBUS_ACCP_LCON:
    case NDBUS_ACCP_STARTMIC:
        return 2;
    case NDBUS_ACCP_LPARP:
    case NDBUS_ACCP_RESTMIC:
    case NDBUS_ACCP_TBUF:
    case NDBUS_ACCP_LAOB16:
    case NDBUS_ACCP_RMPM:
    case NDBUS_ACCP_LAOB32D:
        return 4;
    case NDBUS_ACCP_LSYSPAR:   /* three 16-bit words: error station/OMD, host station/OMD, spare */
    case NDBUS_ACCP_SETTRAC:
        return 6;
    case NDBUS_ACCP_WMPM:
    case NDBUS_ACCP_TESTMPM:
        return 8;
    case NDBUS_ACCP_LMIR:      /* the 128-bit microinstruction register */
        return 16;
    case NDBUS_ACCP_LOCSD:     /* 8 words + control-store address + checksum */
        return 20;
    default:
        return 0;
    }
}

bool ndbus_accp_length_checked_first(uint8_t command)
{
    return command == NDBUS_ACCP_LOCSD || command == NDBUS_ACCP_DCSD;
}

const char *ndbus_accp_command_name(uint8_t command)
{
    switch (command)
    {
    case NDBUS_ACCP_RSSYSPAR:  return "RSSYSPAR(015B)";
    case NDBUS_ACCP_LSYSPAR:   return "LSYSPAR(016B)";
    case NDBUS_ACCP_ECHO:      return "ECHO(017B)";
    case NDBUS_ACCP_RECO:      return "RECO(020B)";
    case NDBUS_ACCP_LPARP:     return "LPARP(021B)";
    case NDBUS_ACCP_VPARP:     return "VPARP(022B)";
    case NDBUS_ACCP_LOCSM:     return "LOCSM(023B)";
    case NDBUS_ACCP_LOCSD:     return "LOCSD(024B)";
    case NDBUS_ACCP_DUCS:      return "DUCS(025B)";
    case NDBUS_ACCP_DCSD:      return "DCSD(026B)";
    case NDBUS_ACCP_RUNTST:    return "RUNTST(033B)";
    case NDBUS_ACCP_STOPMIC:   return "STOPMIC(034B)";
    case NDBUS_ACCP_CONTMIC:   return "CONTMIC(035B)";
    case NDBUS_ACCP_RESTMIC:   return "RESTMIC(036B)";
    case NDBUS_ACCP_ALIVE:     return "ALIVE(037B)";
    case NDBUS_ACCP_LMAR:      return "LMAR(040B)";
    case NDBUS_ACCP_LMIR:      return "LMIR(041B)";
    case NDBUS_ACCP_RMIR:      return "RMIR(042B)";
    case NDBUS_ACCP_TBUF:      return "TBUF(043B)";
    case NDBUS_ACCP_RAIB16:    return "RAIB16(044B)";
    case NDBUS_ACCP_RAIB32D:   return "RAIB32D(045B)";
    case NDBUS_ACCP_TBUS:      return "TBUS(046B)";
    case NDBUS_ACCP_LAOB16:    return "LAOB16(047B)";
    case NDBUS_ACCP_LMODE:     return "LMODE(051B)";
    case NDBUS_ACCP_LCON:      return "LCON(052B)";
    case NDBUS_ACCP_WMPM:      return "WMPM(053B)";
    case NDBUS_ACCP_RMPM:      return "RMPM(054B)";
    case NDBUS_ACCP_SETTRAC:   return "SETTRAC(055B)";
    case NDBUS_ACCP_READSELFT: return "READSELFT(060B)";
    case NDBUS_ACCP_ENKICK:    return "ENKICK(061B)";
    case NDBUS_ACCP_DISKICK:   return "DISKICK(062B)";
    case NDBUS_ACCP_LAOB32D:   return "LAOB32D(063B)";
    case NDBUS_ACCP_LAOB32M:   return "LAOB32M(064B)";
    case NDBUS_ACCP_RAIB32M:   return "RAIB32M(065B)";
    case NDBUS_ACCP_STARTMIC:  return "STARTMIC(066B)";
    case NDBUS_ACCP_AMICTRAP:  return "AMICTRAP(070B)";
    case NDBUS_ACCP_CPURES:    return "CPURES(071B)";
    case NDBUS_ACCP_TESTMPM:   return "TESTMPM(072B)";
    case NDBUS_ACCP_DCCD:      return "DCCD(073B)";
    case NDBUS_ACCP_DUCC:      return "DUCC(074B)";
    case NDBUS_ACCP_PRGMVERS:  return "PRGMVERS(075B)";
    case NDBUS_ACCP_CPUMODEL:  return "CPUMODEL(076B)";
    default:                   return "UNKNOWN";
    }
}
