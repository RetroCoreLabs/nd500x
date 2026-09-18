/*
 * commands.h - debugger shared command library
 * Provides unified command interface for both native CLI and WASM web console
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>
#include "../machine/machine_types.h"

/* Output callback function type
 * Called for each line of text output from command execution
 * @param line    The text line to output
 * @param ctx     User-provided context pointer
 */
typedef void (*cmd_output_fn)(const char* line, void* ctx);

/* Command execution context
 * Provides callbacks for command output and error messages
 */
typedef struct {
    cmd_output_fn output;    /* Normal output callback */
    cmd_output_fn error;     /* Error output callback */
    void* context;           /* User context (e.g., native FILE*, WASM buffer) */
} CmdContext;

/* Execute a debugger command
 * @param m         Machine instance
 * @param cmdline   Command line string to execute
 * @param ctx       Command context with output callbacks
 * @return          0 on success, -1 on error, 1 for quit command
 */
int nd500_cmd_execute(Nd500Machine* m, const char* cmdline, CmdContext* ctx);

/* Parse a string as uint32_t (supports hex with 0x prefix and decimal)
 * @param s              String to parse
 * @param default_value  Value to return if parsing fails
 * @return               Parsed value or default_value
 */
uint32_t nd500_cmd_parse_u32(const char* s, uint32_t default_value);

/* Get list of available commands for autocomplete
 * @return  NULL-terminated array of command name strings
 */
const char** nd500_cmd_get_command_list(void);

/* Get list of subcommands for a given command
 * @param command  Command name (e.g., "show", "set", "bp")
 * @return         NULL-terminated array of subcommand strings, or NULL if none
 */
const char** nd500_cmd_get_subcommands(const char* command);

/* Execute init script from file
 * @param m            Machine instance
 * @param script_path  Path to .init script file
 * @return             0 on success, -1 on error
 */
int nd500_execute_init_script(Nd500Machine* m, const char* script_path);

#endif /* COMMANDS_H */
