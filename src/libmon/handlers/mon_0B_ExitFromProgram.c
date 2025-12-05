/*
 * MON 0B (0 decimal): ExitFromProgram (LEAVE)
 *
 * Terminates the program. Returns to SINTRAN III. Batch jobs continues with the next command.
 *
 * - Background programs close all files not set permanently open. RT programs do not close any files.
 * - RT programs release all reserved devices.
 *
 * Parameters: None
 *
 * K flag: Set if wrong number of parameters
 *
 * IMPLEMENTATION STATUS: VALIDATED
 */

#include "../mon.h"

MonResult mon_0B_ExitFromProgram(MonContext* ctx) {
    /* Log entry */
    mon_log(MON_LOG_INFO, "LEAVE: Program termination requested");

    /* Request halt with reason */
    mon_request_halt(ctx, "Program exited via MON 0B LEAVE");

    /* Set success (K=0) */
    mon_set_success(ctx);

    return MON_SUCCESS;
}
