/*
 * MON 73B (59 decimal): SetMaxBytes (SMAX)
 *
 * Sets the value of the maximum byte pointer in an opened file (i.e. the number of bytes minus 1). The specified number of bytes are stored when the file is closed. The error code 3 is returned if you later try to read beyond this size. Error code 3 means end of file.
 * 
 * - The file must be opened for write.
 * - This monitor call is only relevant for sequential access.
 *
 * Parameters:
 *   [I] FileNumber (INTEGER): input
 *   [I] MaxBytePointer (INTEGER4): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_73B_SetMaxBytes(MonContext* ctx) {
    /* TODO: Implement SetMaxBytes (SMAX) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileNumber");
    MON_LOG_IN_WORD(ctx, 1, "MaxBytePointer");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
