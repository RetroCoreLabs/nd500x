/*
 * MON 74B (60 decimal): SetStartByte (SETBT)
 *
 * Sets the next byte to be read or written in an opened mass-storage file.
 * 
 * - The bytes in a file are numbered upwards from 0.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER): input
 *   [I] BytePointer (INTEGER4): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_74B_SetStartByte(MonContext* ctx) {
    /* TODO: Implement SetStartByte (SETBT) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNumber");
    MON_LOG_IN_WORD(ctx, 1, "BytePointer");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
