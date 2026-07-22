/*
 * MON 400B (256 decimal): ErrorReturn (MACROE)
 *
 * Terminates the program and sets an error code. The error code can be tested by the commands IF-ERROR-MACRO-STOP and IF-ERROR-FULL-STOP commands in the ND-500 Monitor. See the manual ND Linker User Guide and Reference Manual (ND-860289) for details about macros.
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_400B_ErrorReturn(MonContext* ctx) {
    /* TODO: Implement ErrorReturn (MACROE) */

    /* Log input parameters */

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
