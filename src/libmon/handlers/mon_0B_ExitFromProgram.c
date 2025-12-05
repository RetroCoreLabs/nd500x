/*
 * MON 0B (0 decimal): ExitFromProgram (LEAVE)
 *
 * Terminates the program. Returns to SINTRAN III. Batch jobs continues with the next command.
 * 
 * - Background programs close all files not set permanently open. RT programs do not close any files.
 * - RT programs release all reserved devices.
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_0B_ExitFromProgram(MonContext* ctx) {
    /* TODO: Implement ExitFromProgram (LEAVE) */

    /* Log input parameters */

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
