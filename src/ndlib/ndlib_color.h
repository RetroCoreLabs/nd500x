#ifndef NDLIB_COLOR_H
#define NDLIB_COLOR_H

/* ND500X ANSI Color Output Module
 * 
 * Provides colorized output for disassembly and debugging.
 * Color output is auto-detected based on TTY and TERM environment,
 * or can be explicitly controlled via command-line flags.
 */

/* Initialize color system with command-line override
 * ansi_flag: 0=auto-detect, 1=force-enable, -1=force-disable
 */
void ndlib_color_init(int ansi_flag);

/* Color accessor functions
 * Return ANSI escape codes when color is enabled, empty string otherwise.
 */
const char* color_address(void);   /* Gray (bright black) - \033[90m */
const char* color_bytes(void);     /* Yellow - \033[33m */
const char* color_instr(void);     /* Green (bright) - \033[92m */
const char* color_oper(void);      /* White (bright) - \033[97m */
const char* color_branch(void);    /* Red (bright) - \033[91m */
const char* color_label(void);     /* Cyan (bright) - \033[96m */
const char* color_comment(void);   /* Blue (bright) - \033[94m */
const char* color_meta(void);      /* Yellow (bright) - \033[93m */
const char* color_reset(void);     /* Reset to default - \033[0m */

/* Query current color state (for testing/debugging) */
int ndlib_color_enabled(void);

#endif /* NDLIB_COLOR_H */

