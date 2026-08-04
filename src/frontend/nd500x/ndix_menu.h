/*
 * ndix_menu.h - the F12 menu on the local terminal.
 *
 * Same idea as nd100x's F12 menu: while the guest owns the terminal there has
 * to be one key that belongs to the EMULATOR, or there is no way to change
 * anything without killing the process. Two entries for now - switch the
 * terminal between the guest's virtual consoles, and shut NDIX down.
 *
 * ASCII only.
 */

#ifndef NDIX_MENU_H
#define NDIX_MENU_H

struct Nd500Machine;

/* What the caller should do when ndix_menu_run() returns. */
typedef enum {
    NDIX_MENU_RESUME = 0,   /* close the menu, keep NDIX running */
    NDIX_MENU_QUIT          /* the user chose to shut NDIX down   */
} NdixMenuResult;

/* Does <buf> start an F12 key sequence?
 *
 * Returns the number of bytes F12 occupies (so the caller can skip them), 0 if
 * these bytes are definitely not F12, and -1 if they are a PREFIX of one and
 * the caller should read more before deciding. That third case matters: a
 * terminal can split "\x1B[24~" across two read()s, and treating a partial
 * sequence as "not F12" would send the escape to the guest and lose the key. */
int ndix_menu_match_f12(const char* buf, int len);

/* Draw the menu on stdout and act on the key pressed. stdin must already be in
 * raw mode (the guest passthrough loop has it that way). Returns what to do
 * next; the terminal is left exactly as it was found. */
NdixMenuResult ndix_menu_run(struct Nd500Machine* m);

/* Hand this to nd500_debugger_set_guest_key_handler(); it matches F12, runs the
 * menu, and reports back in that function's return convention. */
int ndix_menu_guest_key(struct Nd500Machine* m, const char* buf, int len);

#endif /* NDIX_MENU_H */
