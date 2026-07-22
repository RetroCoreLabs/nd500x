/*
 * MON 75B (61 decimal): GetStartByte (REABT)
 *
 * Gets the number of the next byte to access in a file. The bytes in a file are numbered from 0.
 * 
 * - The file must be opened for sequential access.
 * - You cannot use this monitor call for peripheral files.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER): input
 *   [O] BytePointer (INTEGER4): output
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_75B_GetStartByte(MonContext* ctx) {
    /* TODO: Implement GetStartByte (REABT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
