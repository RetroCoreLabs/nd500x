/*
 * nd500_mon_sintran.h - the SINTRAN MON-call emulation seam.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * WHY THIS FILE EXISTS (2026-08-08)
 * ---------------------------------
 * Two very different things arrive on segment 31, and only one of them is
 * needed to run NDIX:
 *
 *   1. MON 600 (octal; CALLG offset 0x180) - the NDIX ND-100 front-end call,
 *      "fecall". The NDIX kernel issues it from machine/locore.c:227 as
 *
 *          callg $0xf8000180,$4,b.20,b.24,b.28,b.32
 *
 *      and all THREE kernel entry points (_fecall, _feinit_fecall and
 *      _feexit_fecall) funnel into that single instruction. It is the ONLY
 *      monitor call NDIX ever makes: every device - disk (io/di.c), tape
 *      (io/ct.c, io/mt.c), terminals (io/mx.c) and XMSG (if/xg.c) - reaches
 *      the front end through it. It is served entirely inside nd500x, by
 *      src/cpu/nd500_fecall.c, and depends on nothing external.
 *
 *   2. Every other MON number - SINTRAN III monitor-call emulation, used when
 *      running DOM programs with no real ND-100 present. That is served by the
 *      external ndmonlib submodule, which is a separate (currently private)
 *      repository.
 *
 * Putting (2) behind this one header is what keeps nd500_cpu and nd500_machine
 * free of any ndmonlib dependency. Built with -DND500X_WITH_NDMON=OFF the stub
 * implementation is linked instead: the emulator still boots NDIX (because
 * NDIX only needs MON 600) and the two core libraries can be linked into
 * another host - nd100x's WASM build in particular - with no submodule at all.
 *
 * Two implementations satisfy this header; exactly one is compiled in:
 *   nd500_mon_sintran.c       - the real thing, requires ndmonlib  (ON)
 *   nd500_mon_sintran_stub.c  - refuses politely, no dependency    (OFF)
 *
 * Background and the decision that led here:
 *   E:\Dev\Ronny\NDIX-C\notes\docs\
 *       PLAN_MACHINE_CONFIG_UI_AND_ND500_INTEGRATION_2026-08-08.md  (section 6A)
 */

#ifndef ND500_MON_SINTRAN_H
#define ND500_MON_SINTRAN_H

#include <stdint.h>
#include "cpu_protos.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Is SINTRAN MON emulation compiled in?
 * 1 = the real ndmonlib-backed implementation, 0 = the stub.
 * Useful for banners and for tests that must skip DOM/SINTRAN scenarios.
 */
int nd500_mon_sintran_available(void);

/*
 * Service one SINTRAN monitor call arriving on segment 31.
 *
 * MON 600 must NEVER reach here - nd500_indirect.c intercepts it first and
 * calls nd500_fecall() directly, because the front-end call needs full
 * cpu/machine/DMA access that the generic MON registry does not offer.
 *
 * @param cpu               CPU state.
 * @param mon_number        The MON number (the CALLG offset within segment 31).
 * @param instruction_addr  PC of the CALLG, used for logging and for rewinding
 *                          the PC when a blocking read suspends the process.
 * @param arg_count         Number of arguments.
 * @param arg_addresses     Argument EFFECTIVE ADDRESSES (args are by address).
 * @param out_resolved      Out: address to continue at.
 *
 * @return an IndirectCallResult value (INDIRECT_HANDLED, INDIRECT_WAIT,
 *         INDIRECT_ERROR, INDIRECT_BREAK), matching what
 *         nd500_check_indirect_call must return to its caller.
 */
int nd500_mon_sintran_call(Nd500Cpu* cpu,
                           uint32_t mon_number,
                           uint32_t instruction_addr,
                           uint32_t arg_count,
                           const uint32_t* arg_addresses,
                           uint32_t* out_resolved);

/* ---- SINTRAN file-table lifecycle (machine/io.c) ---------------------------
 * The emulated SINTRAN file system tables behind MON 50/43/122/123 and friends.
 * No-ops in the stub build - NDIX has its own file system and never uses these. */
void nd500_mon_files_init(void);
void nd500_mon_files_reset(void);

/* ---- Console capture (machine/debug_api.c) --------------------------------
 * Route MON console output into the debugger's trace file. Pass NULL to restore
 * the default console. No-op in the stub build. The callback signature matches
 * ndmonlib's ConsoleIO.write_char so the caller needs no ndmonlib types. */
void nd500_mon_set_trace_console(void (*write_char)(void* ctx, int ch));

/* ---- MON name lookup (disassembly annotation) -----------------------------
 * Return the octal spelling / short name / long name of a MON number, or NULL
 * when unknown. The stub returns NULL for all three, which every caller
 * already handles (the disassembly simply prints "[unregistered]"). */
/* ---- MON debug logging (instructions/CALL/Call.c) -------------------------
 * Argument tracing for calls into segment 31, at ndmonlib's DEBUG level so it
 * obeys the same `mon log` switches as the handlers themselves. A no-op in the
 * stub build - there is no MON log to write to. Kept variadic so the call site
 * reads exactly as it did before the seam existed. */
void nd500_mon_log_debug(const char* fmt, ...);

const char* nd500_mon_octal(uint32_t mon_number);
const char* nd500_mon_name(uint32_t mon_number);
const char* nd500_mon_long_name(uint32_t mon_number);

#ifdef __cplusplus
}
#endif

#endif /* ND500_MON_SINTRAN_H */
