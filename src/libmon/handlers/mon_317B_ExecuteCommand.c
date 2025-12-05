/*
 * MON 317B (207 decimal): ExecuteCommand (UECOM)
 *
 * Executes a SINTRAN III command. Specify the command name and the parameters as a text string.
 * 
 * - An error message is output if an error occurs. The program does not terminate.
 * - Some commands may destroy your program. Commands which affect your program?s memory area should be used with care.
 * - Some commands have output, e.g. @LIST-FILES. This is displayed on the terminal.
 * - Use SuspendProgram to wait a second between two ExecuteCommands which depend on each other, e.g. CreateFile and OpenFile.
 * - It may be advisable to use @enable-escape before this call, to avoid having problems terminating some commands.
 *
 * Parameters:
 *   [I] Command (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_317B_ExecuteCommand(MonContext* ctx) {
    /* TODO: Implement ExecuteCommand (UECOM) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "Command");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
