/*
 * nd_tty.h - cross-platform local-terminal shim (POSIX termios vs Win32 console).
 *
 * nd500x drives the host terminal directly in two places:
 *
 *   - src/debugger/debugger.c   guest_passthrough(): raw mode, a 200 ms timed
 *                               wait on stdin, then a raw read.
 *   - src/frontend/nd500x/nd500x_shell.c: save cooked mode at start, restore it
 *                               after a DOM run has left the console raw.
 *
 * Both were written against <termios.h> + select(), neither of which exists in
 * the MinGW/Win32 headers. This shim is the same idea expressed twice:
 *
 *   POSIX     tcgetattr/tcsetattr + select(STDIN_FILENO)
 *   Windows   GetConsoleMode/SetConsoleMode + WaitForSingleObject on the
 *             console input handle
 *
 * Why the Windows side matters beyond "it compiles": the console must be put in
 * VIRTUAL TERMINAL INPUT mode, otherwise function keys arrive as KEY_EVENT
 * records carrying no character and a plain read() never sees them at all. F12
 * (the NDIX menu hot key) reaches the emulator as ESC [ 2 4 ~ only in that
 * mode - which is exactly the byte sequence ndix_menu_match_f12() looks for, so
 * one console-mode flag is the difference between the menu working and the key
 * doing nothing. The matching output flag turns on ANSI escape handling, which
 * the guest's own screen output needs.
 *
 * Everything here talks to stdin/stdout only. Sockets are net_compat.h's job.
 */
#ifndef ND_TTY_H
#define ND_TTY_H

#ifdef _WIN32
#  include <windows.h>
#else
#  include <termios.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Saved terminal settings. Treat the contents as opaque - the layout differs
 * per platform and only nd_tty.c should look inside. Declared in the header
 * rather than heap-allocated so callers can keep it on the stack, the way the
 * old "struct termios saved;" locals did. */
typedef struct {
    int valid;                  /* 0 if nothing was captured (not a terminal) */
#ifdef _WIN32
    DWORD in_mode;              /* console input mode as found */
    DWORD out_mode;             /* console output mode as found */
    int   stdin_fmode;          /* CRT translation mode of fd 0, to restore */
#else
    struct termios t;
#endif
} nd_tty_mode;

/* Is stdin an interactive terminal? Scripted/piped input answers 0, and every
 * caller uses that to skip raw mode entirely. */
int nd_tty_stdin_is_terminal(void);

/* Capture the current terminal settings into *saved. Returns 0 on success, -1
 * if stdin is not a terminal or the query failed; *saved is marked invalid in
 * that case, so nd_tty_restore() on it is a harmless no-op. */
int nd_tty_save(nd_tty_mode* saved);

/* Put the terminal back the way *saved found it. Safe to call with an invalid
 * or NULL saved state. Anything already typed ahead is kept. */
void nd_tty_restore(const nd_tty_mode* saved);

/* Throw away input that has arrived but not been read yet.
 *
 * The shell calls this when a DOM run ends: whatever was typed at the guest
 * while it had the terminal is meant for the guest, not for the shell prompt
 * that is about to appear. This is the TCSAFLUSH half of the old
 * tcsetattr(..., TCSAFLUSH, ...) restore, split out because the other caller
 * (the debugger) deliberately wants type-ahead preserved. */
void nd_tty_flush_input(void);

/* Switch stdin to raw: no line buffering, no local echo, and no host-side
 * stealing of the control characters the GUEST needs to see.
 *
 * pass_signals != 0 leaves Ctrl-C etc. as host signals (the ND500X_HOST_SIGINT
 * escape hatch). pass_signals == 0 - the default - delivers those bytes to the
 * guest instead, because NDIX has its own idea of what Ctrl-C, Ctrl-S/Ctrl-Q
 * (CSTOP/CSTART) and Ctrl-V (CLNEXT) mean.
 *
 * *base must be a state captured by nd_tty_save(); raw mode is derived from it
 * so unrelated flags survive untouched. Returns 0 on success, -1 on failure. */
int nd_tty_set_raw(const nd_tty_mode* base, int pass_signals);

/* Wait up to timeout_ms for stdin to have input. Returns 1 if a read will find
 * something, 0 on timeout, -1 on error. A negative timeout waits forever.
 *
 * The timeout is what lets the passthrough loop notice the machine has stopped
 * even when nobody is typing. */
int nd_tty_wait_readable(int timeout_ms);

/* Raw read from stdin. Returns bytes read, 0 at EOF, -1 on error. This is
 * read(2) on POSIX and _read() on Windows - deliberately NOT stdio, because a
 * FILE* would re-introduce the line buffering raw mode just removed. */
int nd_tty_read(void* buf, int max);

/* Turn on ANSI escape processing for stdout, so the colour and cursor control
 * the emulator and the guest emit render instead of showing up as gibberish.
 * No-op on POSIX, where terminals have always done this. Safe to call more than
 * once; failure is ignored, since an old console just shows escape codes rather
 * than breaking. */
void nd_tty_enable_ansi_output(void);

#ifdef __cplusplus
}
#endif

#endif /* ND_TTY_H */
