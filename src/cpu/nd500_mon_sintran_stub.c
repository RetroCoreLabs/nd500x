/*
 * nd500_mon_sintran_stub.c - the no-ndmonlib implementation of the seam.
 *
 * Compiled instead of nd500_mon_sintran.c when the build is configured with
 * -DND500X_WITH_NDMON=OFF. See nd500_mon_sintran.h for the full rationale; the
 * short version is that NDIX needs exactly ONE monitor call - MON 600, the
 * front-end call - and that one is served by nd500_fecall.c with no external
 * dependency. Everything else on segment 31 is SINTRAN emulation, which an
 * NDIX-only or embedded-in-nd100x build does not want to carry.
 *
 * So this file does not fake SINTRAN. It stops the machine with a message that
 * says exactly why, which is far better than silently returning success and
 * letting a DOM program wander off.
 */

#include "nd500_mon_sintran.h"
#include "nd500_indirect.h"          /* IndirectCallResult */
#include "../machine/machine_protos.h"
#include <stdio.h>

int nd500_mon_sintran_available(void) {
    return 0;
}

int nd500_mon_sintran_call(Nd500Cpu* cpu,
                           uint32_t mon_number,
                           uint32_t instruction_addr,
                           uint32_t arg_count,
                           const uint32_t* arg_addresses,
                           uint32_t* out_resolved)
{
    (void)arg_addresses;
    (void)instruction_addr;

    /* Stop rather than continue. A MON call that silently "succeeds" corrupts
     * the guest in a way that is very hard to trace back to a build option. */
    if (cpu && cpu->machine) {
        cpu->machine->run_flag = 0;
        cpu->machine->stop_reason = STOP_MON_UNIMPLEMENTED;
        cpu->machine->stop_addr = cpu->PC;
        cpu->machine->stop_data = mon_number;
    }
    nd500_dbg_flush_console_output();

    fprintf(stderr,
            "[STOP] MON %u (octal %o) with %u args: this build has no SINTRAN\n"
            "       MON emulation (configured with ND500X_WITH_NDMON=OFF).\n"
            "       Only MON 600 (the NDIX front-end call) is available.\n"
            "       Reconfigure with -DND500X_WITH_NDMON=ON and the ndmonlib\n"
            "       submodule checked out to run SINTRAN DOM programs.\n",
            mon_number, mon_number, arg_count);

    if (out_resolved && cpu) *out_resolved = cpu->pending_call_return_address;
    return INDIRECT_BREAK;
}

/* SINTRAN file tables: nothing to set up when there is no SINTRAN. NDIX brings
 * its own file system and never touches these. */
void nd500_mon_files_init(void)  { }
void nd500_mon_files_reset(void) { }

/* No MON console to capture. */
void nd500_mon_set_trace_console(void (*write_char)(void* ctx, int ch)) {
    (void)write_char;
}

/* No MON log to write into. */
void nd500_mon_log_debug(const char* fmt, ...) {
    (void)fmt;
}

/* No MON registry, so no names. Every caller already handles NULL by printing
 * "[unregistered]" - see debug_api.c format_mon_call_info(). */
const char* nd500_mon_octal(uint32_t mon_number)     { (void)mon_number; return 0; }
const char* nd500_mon_name(uint32_t mon_number)      { (void)mon_number; return 0; }
const char* nd500_mon_long_name(uint32_t mon_number) { (void)mon_number; return 0; }
