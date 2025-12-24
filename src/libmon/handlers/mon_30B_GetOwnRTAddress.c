/*
 * MON 30B [GETRT/GetOwnRTAddress]
 *
 * Gets the address of the calling program's RT description. Background
 * programs get the RT description address of the RT program which controls
 * the terminal.
 *
 * Parameters:
 *   [O] RTDescrAddress (INTEGER): RT description address (returned in W1)
 *
 * Note: This is a stub implementation that returns a fixed address.
 * Real RT scheduling is not implemented.
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"

/* Simulated RT description address */
#define STUB_RT_DESCRIPTION_ADDR 0x10000000

MonResult mon_30B_GetOwnRTAddress(MonContext* ctx) {
    /* Return fixed RT description address in W1 */
    ctx->set_error_code(ctx->cpu, STUB_RT_DESCRIPTION_ADDR);

    mon_log(MON_LOG_DEBUG, MON_ID_30B ": IN: (none)");
    mon_log(MON_LOG_DEBUG, MON_ID_30B ": OUT: RTDescrAddress=0x%08X",
            STUB_RT_DESCRIPTION_ADDR);

    mon_set_success(ctx);
    return MON_SUCCESS;
}
