/*
 * MON 340B (224 decimal): ReadSystemRecord (RSREC)
 *
 * Used to read the system record into a buffer.
 *
 * Parameters:
 *   [I] RecordType (INTEGER2): input
 *   [I] AddressOrNumber (INTEGER2): input
 *   [O] Buffer (INTEGER2[]): output
 *   [I] Format (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_340B_ReadSystemRecord(MonContext* ctx) {
    /* TODO: Implement ReadSystemRecord (RSREC) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "RecordType");
    MON_LOG_IN_WORD(ctx, 1, "AddressOrNumber");
    MON_LOG_IN_WORD(ctx, 3, "Format");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
