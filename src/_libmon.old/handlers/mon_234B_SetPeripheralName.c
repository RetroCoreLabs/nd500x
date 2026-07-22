/*
 * MON 234B (156 decimal): SetPeripheralName (SPEFI)
 *
 * Defines a peripheral file, e.g. a printer. You connect a file name to the logical device number of the peripheral.
 * 
 * - The file name should exist in advance, but with no file type. Otherwise you may include the file name in double quotes (\"...\"). An empty file type is default.
 *
 * Parameters:
 *   [I] FileName (STRING): input
 *   [I] DeviceNumber (INTEGER2): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_234B_SetPeripheralName(MonContext* ctx) {
    /* TODO: Implement SetPeripheralName (SPEFI) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FileName");
    MON_LOG_IN_WORD(ctx, 1, "DeviceNumber");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
