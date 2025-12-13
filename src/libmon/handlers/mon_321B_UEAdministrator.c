/*
 * MON 321B (209 decimal): UEAdministrator (UEADM)
 *
 * DEPRECATED: This MON call is no longer supported.
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"

MonResult mon_321B_UEAdministrator(MonContext* ctx) {
    mon_log(MON_LOG_WARN, "MON 321B UEADM: Deprecated MON call - returning error");

    /* Return error - deprecated call */
    mon_set_error(ctx, 52);  /* Invalid parameter */
    return MON_ERROR;
}
