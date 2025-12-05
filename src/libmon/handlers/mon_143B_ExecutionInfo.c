/*
 * MON 143B (99 decimal): ExecutionInfo (RSIO)
 *
 * Gets information about the execution of the calling program. You are told
 * whether the program executes interactively, as a batch or mode job, or as
 * an RT program. The monitor call returns some additional information for
 * non-RT programs, consisting of the command input file, the command output
 * file, and the directory index and user index of the program's owner.
 *
 * Parameters (all are 32-bit WORD on ND-500):
 *   [O] ExecutionMode (W INTEGER): Execution mode:
 *       0 = interactive program
 *       1 = batch job
 *       2 = mode job
 *       3 = RT program
 *   [O] InputDev (W INTEGER): Logical device number for command input.
 *       Terminal number for interactive, file number for batch/mode.
 *   [O] OutputDev (W INTEGER): Logical device number for command output.
 *       Terminal number for interactive, file number for batch/mode.
 *   [O] UserIndex (W INTEGER): Directory and user index of program's owner.
 *       Bits 8-15 = directory index, bits 0-7 = user index.
 *
 * Note: On ND-500, INTEGER = 32-bit Word (W type).
 *       On ND-100, INTEGER = 16-bit Halfword (H type).
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
    uint32_t exec_mode;
    uint32_t input_dev;
    uint32_t output_dev;
    uint32_t user_index;

    /* Set up execution environment values
     * For the emulator, we simulate an interactive terminal session.
     */
    exec_mode = DEFAULT_EXEC_MODE;          /* Interactive program */
    input_dev = DEFAULT_TERMINAL;           /* Terminal 1 for input */
    output_dev = DEFAULT_TERMINAL;          /* Terminal 1 for output */
    user_index = (DEFAULT_DIRECTORY_INDEX << 8) | DEFAULT_USER_INDEX;

    /* Write output parameters as 32-bit words (ND-500 INTEGER = W) */
    if (ctx->arg_count >= 1) {
        mon_write_param_word(ctx, 0, exec_mode);
    }
    if (ctx->arg_count >= 2) {
        mon_write_param_word(ctx, 1, input_dev);
    }
    if (ctx->arg_count >= 3) {
        mon_write_param_word(ctx, 2, output_dev);
    }
    if (ctx->arg_count >= 4) {
        mon_write_param_word(ctx, 3, user_index);
    }

    /* Log the result */
    mon_log(MON_LOG_INFO, "MON 143B RSIO: mode=%u, input=%u, output=%u, user_idx=0x%08X (dir=%u, user=%u)",
            exec_mode, input_dev, output_dev, user_index,
            (user_index >> 8) & 0xFF, user_index & 0xFF);

    /* Set success (K=0) */
    mon_set_success(ctx);

    return MON_SUCCESS;
}
