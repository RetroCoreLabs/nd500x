/*
 * MON 321B [UEADM/UEAdministrator]
 *
 * DEPRECATED: This MON call is no longer supported.
 *
 * Reference: SINTRAN III Monitor Calls (ND-860228.2 EN)
 */

#include "../mon.h"
#include "../mon_log.h"
#include "../mon_errors.h"

MonResult mon_321B_UEAdministrator(MonContext* ctx) {
    mon_log(MON_LOG_WARN, MON_ID_321B ": Deprecated MON call - returning error");

    /* Return error - deprecated call */
    mon_set_error(ctx, MON_ERR_ILLEGAL_PARAMETER);  /* 174B Illegal parameter */
    return MON_ERROR;
}
