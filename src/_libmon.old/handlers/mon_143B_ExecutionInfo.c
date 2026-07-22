/*
 * MON 143B [RSIO/ExecutionInfo]
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
#include "../mon_log.h"

/* Default execution environment settings.
 *
 * INPUT DEV = 1 (the TERMINAL), changed 2026-07-16 - previously 0.
 *
 * Its own documented contract (above) says InputDev is the "Terminal number for
 * interactive, file number for batch/mode". We report ExecutionMode = 0
 * (interactive) and OutputDev = 1 (terminal), so InputDev = 0 (the SINTRAN
 * command buffer = the BATCH channel) was self-inconsistent: it told an
 * interactive program to take its commands from the batch channel.
 *
 * Byte-verified consequence of the old value: the ND LINKER believed its command
 * input was device 0, issued 1B INBT on it, found it empty and blocked forever.
 * Feeding device 0 instead forced it into its BATCH dialogue ("Batch abortion
 * (Yes,No)Yes") where it spun without ever consuming the fed commands.
 *
 * COUPLED CHANGE - do not ship this alone: with InputDev = 1 the linker takes
 * its interactive path and immediately calls MON 511B DVIO. Before 511B existed
 * that traded a hang for an unimplemented-MON stop, which is why this was probed
 * and REVERTED twice (2026-07-15). It lands only together with
 * handlers/mon_511B_DVIO.c.
 */
#define DEFAULT_EXEC_MODE       0       /* Interactive program (0=interactive, 3=RT) */
#define DEFAULT_INPUT_DEV       1       /* Terminal (interactive command input) */
#define DEFAULT_OUTPUT_DEV      1       /* Output device 1 (terminal) */
#define DEFAULT_DIRECTORY_INDEX 1       /* Default directory */
#define DEFAULT_USER_INDEX      1       /* Default user (SYSTEM or RT) */

MonResult mon_143B_ExecutionInfo(MonContext* ctx) {
    uint32_t exec_mode;
    uint32_t input_dev;
    uint32_t output_dev;
    uint32_t user_index;

    /* Set up execution environment values
     * For the emulator, we simulate an interactive program environment.
     * This allows programs to read from the command buffer via InByte(0).
     */
    exec_mode = DEFAULT_EXEC_MODE;          /* Interactive program */
    input_dev = DEFAULT_INPUT_DEV;          /* Input device 0 */
    output_dev = DEFAULT_OUTPUT_DEV;        /* Output device 1 */
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
    mon_log(MON_LOG_INFO, MON_ID_143B ": IN: (none)");
    mon_log(MON_LOG_INFO, MON_ID_143B ": OUT: mode=%o, input=%o, output=%o, user_idx=%o",
            exec_mode, input_dev, output_dev, user_index);

    /* Set success (K=0) */
    mon_set_success(ctx);

    return MON_SUCCESS;
}
