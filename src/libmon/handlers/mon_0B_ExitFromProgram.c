/*
 * MON 0B (0 decimal): ExitFromProgram (LEAVE)
 *
 * Terminates the program. Returns to SINTRAN III. Batch jobs continues with the next command.
 *
 * - Background programs close all files not set permanently open. RT programs do not close any files.
 * - RT programs release all reserved devices.
 *
 * Implementation:
 *   Requests CPU halt to stop execution. In emulator context, this ends the program.
 */

#include "../mon.h"

MonResult mon_0B_ExitFromProgram(MonContext* ctx) {
    /* Request CPU halt - program is exiting */
    mon_request_halt(ctx, "Program exit (MON 0B LEAVE)");

    mon_log(MON_LOG_INFO, "MON 0B LEAVE: Program terminated");

    mon_set_success(ctx);
    return MON_SUCCESS;
}
