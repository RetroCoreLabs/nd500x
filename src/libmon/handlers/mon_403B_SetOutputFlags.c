/*
 * MON 403B (259 decimal): SetOutputFlags (WFLAG)
 *
 * ND-100 and ND-500 programs may communicate through two 32-bit flag arrays. You can use the flags as you want. SetOutputFlags writes to the output flags. The ND-100 reads these flags with the monitor call ND500Function. See the manual ND Linker User Guide and Reference Manual (ND-860289).
 * 
 * - You store the last values written to the flags. There is no queue.
 *
 * Parameters:
 *   [I] Value (INTEGER): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_403B_SetOutputFlags(MonContext* ctx) {
    /* TODO: Implement SetOutputFlags (WFLAG) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "Value");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
