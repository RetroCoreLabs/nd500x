/*
 * MON 70B (56 decimal): CallCommand (COMMND)
 *
 * Executes a SINTRAN III command from a program. The program terminates if an error occurs in the command.
 * 
 * - You are advised to use the newer monitor call, ExecuteCommand (UECOM), instead of CallCommand.
 * - Some commands may destroy your program. Use care with commands which affect your program's memory area.
 * - Note that the program may terminate if an error occurs. Use ExecuteCommand (UECOM) to avoid this problem.
 * - For commands with output, but without an output file as a parameter, e.g. @WHO, output is displayed automatically on the terminal of the user executing the program.
 * - Use SuspendProgram to wait a specified time interval between two CallCommands which depend on each other, e.g. CreateFile and OpenFile.
 * - The following commands are allowed from ND-500 programs: @DATCL, @COPY, @COPY-FILE, @SCHEDULE, @HOLD, @TERMINAL-MODE, @OPERATOR, @WAIT-FOR-OPERATOR, @SET-TERMINAL-TYPE, @GET-TERMINAL-TYPE, @CREATE-FILE, @EXPAND-FILE, @DELETE-FILE, @RENAME-FILE, @LIST-FILE, @FILE-STATISTICS, @OPEN-FILE, @CONNECT-FILE, @SET-FILE-ACCESS, @CLOSE-FILE, @LIST-OPEN-FILE, @SET-BLOCK-SIZE, @SET-PERMANENT-OPEN, @SET-BYTE-POINTER, @SET-BLOCK-POINTER, @APPEND-SPOOLING-FILE, @DELETE-SPOOLING-FILE, @SET-TEMPORARY-FILE, and @SCRATCH-OPEN.
 *
 * Parameters:
 *   [I] Command (STRING): input
 *
 * AUTO-GENERATED STUB - Implementation required
 */

#include "../mon.h"

MonResult mon_70B_CallCommand(MonContext* ctx) {
    /* TODO: Implement CallCommand (COMMND) */

    /* Log input parameters */
    MON_LOG_IN_WORD(ctx, 0, "Command");

    /* Implementation goes here */

    /* Set error - not yet implemented */
    mon_set_error(ctx, -1);

    return MON_ERROR;
}
