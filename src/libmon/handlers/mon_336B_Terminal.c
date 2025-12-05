/*
 * MON 336B (222 decimal): Terminal (IOMTY)
 *
 * This I/O multifunction monitor call is used to change the attributes of terminal and terminal access device (TAD) input/output. It is also used to configure NET/One interfaces and SCSI disks.
 * 
 * This monitor call needs a varying number of input and output parameters depending upon function. All parameters are therefore placed in an array.
 *
 * Parameters:
 *   [I] FunctionCode (INTEGER2): input
 *   [I] ArrayLength (INTEGER2): input
 *   [IO] ParameterArray (INTEGER2[]): in/out
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_336B_Terminal(MonContext* ctx) {
    /* TODO: Implement Terminal (IOMTY) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "FunctionCode");
    MON_LOG_IN_WORD(ctx, 1, "ArrayLength");
    MON_LOG_IN_WORD(ctx, 2, "ParameterArray");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
