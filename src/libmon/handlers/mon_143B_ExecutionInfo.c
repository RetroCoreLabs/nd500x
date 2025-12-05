/*
 * MON 143B (99 decimal): ExecutionInfo (RSIO)
 *
 * Gets information about the execution of the calling program. You are told
 * whether the program executes interactively, as a batch or mode job, or as
 * an RT program. The monitor call returns some additional information for
 * non-RT programs, consisting of the command input file, the command output
 * file, and the directory index and user index of the program's owner.
 *
 * Parameters:
 *   [O] ExecutionMode (INTEGER): Execution mode:
 *       0 = interactive program
 *       1 = batch job
 *       2 = mode job
 *       3 = RT program
 *   [O] InputDev (INTEGER): Logical device number for command input.
 *       Terminal number for interactive, file number for batch/mode.
 *   [O] OutputDev (INTEGER): Logical device number for command output.
 *       Terminal number for interactive, file number for batch/mode.
 *   [O] UserIndex (INTEGER): Directory and user index of program's owner.
 *       Bits 8-15 = directory index, bits 0-7 = user index.
 *
 * Reference: ND-860228.2 EN (SINTRAN III Monitor Calls)
 */

#include "../mon.h"

/* Default execution environment settings */
#define DEFAULT_EXEC_MODE       0       /* Interactive program */
#define DEFAULT_TERMINAL        1       /* Terminal 1 */
#define DEFAULT_DIRECTORY_INDEX 1       /* Default directory */
#define DEFAULT_USER_INDEX      1       /* Default user (SYSTEM or RT) */

MonResult mon_143B_ExecutionInfo(MonContext* ctx) {
    uint16_t exec_mode;
    uint16_t input_dev;
    uint16_t output_dev;
    uint16_t user_index;

    /* Set up execution environment values
     * For the emulator, we simulate an interactive terminal session.
     */
    exec_mode = DEFAULT_EXEC_MODE;          /* Interactive program */
    input_dev = DEFAULT_TERMINAL;           /* Terminal 1 for input */
    output_dev = DEFAULT_TERMINAL;          /* Terminal 1 for output */
    user_index = (DEFAULT_DIRECTORY_INDEX << 8) | DEFAULT_USER_INDEX;

    /* Write output parameters */
    if (ctx->arg_count >= 1) {
        mon_write_param_halfword(ctx, 0, exec_mode);
    }
    if (ctx->arg_count >= 2) {
        mon_write_param_halfword(ctx, 1, input_dev);
    }
    if (ctx->arg_count >= 3) {
        mon_write_param_halfword(ctx, 2, output_dev);
    }
    if (ctx->arg_count >= 4) {
        mon_write_param_halfword(ctx, 3, user_index);
    }

    /* Log the result */
    mon_log(MON_LOG_INFO, "MON 143B RSIO: mode=%u, input=%u, output=%u, user_idx=0x%04X (dir=%u, user=%u)",
            exec_mode, input_dev, output_dev, user_index,
            (user_index >> 8) & 0xFF, user_index & 0xFF);

    /* Set success (K=0) */
    mon_set_success(ctx);

    return MON_SUCCESS;
}
