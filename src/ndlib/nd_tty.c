/*
 * nd_tty.c - cross-platform local-terminal shim. See include/nd_tty.h for what
 * each function promises; this file is the two implementations of it.
 */
#include "nd_tty.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <io.h>          /* _isatty, _read, _setmode */
#  include <fcntl.h>       /* _O_BINARY, _O_TEXT */
#  include <windows.h>
#else
#  include <unistd.h>
#  include <sys/select.h>
#  include <errno.h>
#endif

/* ------------------------------------------------------------------------- */
/* Windows                                                                    */
/* ------------------------------------------------------------------------- */
#ifdef _WIN32

/* These two are Windows 10 (1511) and later. Define them if the toolchain's
 * headers predate that, so the source still builds against an older MinGW. The
 * values are fixed by the API and will not change. */
#ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
#  define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
#endif
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#  define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

static HANDLE tty_in(void)  { return GetStdHandle(STD_INPUT_HANDLE); }
static HANDLE tty_out(void) { return GetStdHandle(STD_OUTPUT_HANDLE); }

/* Is this key one that produces no input bytes at all, however it is pressed?
 *
 * Pressing Shift on its own queues a key-down record but yields nothing to
 * read, so treating it as "input is available" would send the caller into a
 * read() that blocks until the user types something real. Every other key does
 * produce bytes in virtual-terminal input mode, including the function and
 * cursor keys, which arrive as ANSI escape sequences. */
static int is_bare_modifier(WORD vk) {
    switch (vk) {
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
        case VK_MENU: case VK_LMENU: case VK_RMENU:     /* Alt */
        case VK_LWIN: case VK_RWIN: case VK_APPS:
        case VK_CAPITAL: case VK_NUMLOCK: case VK_SCROLL:
            return 1;
        default:
            return 0;
    }
}

/* A console handle answers GetConsoleMode; a pipe or a file does not. That is a
 * sharper test than _isatty(), which also says "yes" to the NUL device. */
static int is_console(HANDLE h) {
    DWORD mode;
    return (h != NULL && h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode));
}

int nd_tty_stdin_is_terminal(void) {
    return is_console(tty_in()) ? 1 : 0;
}

int nd_tty_save(nd_tty_mode* saved) {
    if (!saved) return -1;
    memset(saved, 0, sizeof *saved);
    if (!GetConsoleMode(tty_in(), &saved->in_mode)) return -1;
    /* The output mode is saved too, because nd_tty_enable_ansi_output()
     * changes it and restoring only the input half would leave the console in
     * a state this program created. */
    if (!GetConsoleMode(tty_out(), &saved->out_mode)) saved->out_mode = 0;
    /* -1 means "not changed"; nd_tty_set_raw() fills this in if it switches
     * fd 0 to binary, so nd_tty_restore() knows whether to put it back. */
    saved->stdin_fmode = -1;
    saved->valid = 1;
    return 0;
}

void nd_tty_restore(const nd_tty_mode* saved) {
    if (!saved || !saved->valid) return;
    SetConsoleMode(tty_in(), saved->in_mode);
    if (saved->out_mode) SetConsoleMode(tty_out(), saved->out_mode);
    if (saved->stdin_fmode != -1) _setmode(0, saved->stdin_fmode);
}

void nd_tty_flush_input(void) {
    HANDLE h = tty_in();
    if (is_console(h)) FlushConsoleInputBuffer(h);
}

int nd_tty_set_raw(const nd_tty_mode* base, int pass_signals) {
    DWORD mode;
    if (!base || !base->valid) return -1;

    /* Put fd 0 in BINARY mode. The CRT opens it in TEXT mode, where _read()
     * rewrites the byte stream on its way through, and raw mode means raw:
     *
     *   - CR LF is folded to a single LF, and a lone CR - which is exactly what
     *     the console sends for Enter - can be held back waiting to see whether
     *     an LF follows. That is the measured "the first Enter does nothing,
     *     press it twice" symptom.
     *   - Ctrl-Z (0x1A) is taken as END OF FILE and ends input for good. The
     *     guest wants that byte; NDIX has its own meaning for it.
     *
     * The cast away from const is deliberate and confined to this one field:
     * the mode has to be recorded somewhere nd_tty_restore() can find it, and
     * *base is the state that call is given. */
    {
        int prev = _setmode(0, _O_BINARY);
        if (prev != -1 && prev != _O_BINARY)
            ((nd_tty_mode*)base)->stdin_fmode = prev;
    }

    mode = base->in_mode;

    /* ENABLE_LINE_INPUT is the console's line editor: without clearing it,
     * nothing is delivered until Enter. ENABLE_ECHO_INPUT is the local echo the
     * guest is responsible for. Both are the direct equivalent of clearing
     * ICANON and ECHO on POSIX. */
    mode &= ~(DWORD)(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);

    /* ENABLE_PROCESSED_INPUT is where Ctrl-C becomes a host signal instead of a
     * byte. Same trade-off as ISIG on POSIX: NDIX wants the 0x03 itself. */
    if (!pass_signals) mode &= ~(DWORD)ENABLE_PROCESSED_INPUT;

    /* Mouse and window-resize records would sit in the same input queue as
     * keystrokes and wake the wait below for no reason. Nothing here uses
     * either, so keep them out of the queue entirely. */
    mode &= ~(DWORD)(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT);

    /* The important one. In VT input mode the console translates the keys that
     * have no character - function keys, arrows, Home/End - into the ANSI
     * escape sequences a Unix terminal would send, so a plain read() sees them.
     * Without it F12 produces a key record with no character and the NDIX menu
     * hot key is simply unreachable on Windows. */
    mode |= (DWORD)ENABLE_VIRTUAL_TERMINAL_INPUT;

    if (!SetConsoleMode(tty_in(), mode)) return -1;
    nd_tty_enable_ansi_output();
    return 0;
}

int nd_tty_wait_readable(int timeout_ms) {
    HANDLE h = tty_in();
    DWORD  deadline_ms;
    DWORD  start;

    if (!is_console(h)) {
        /* A pipe or file: Windows offers no dependable timed wait for these,
         * and every caller only runs this loop on an interactive terminal
         * anyway. Claim readable and let the read block - the same answer
         * select() would give for a file. */
        return 1;
    }

    start       = GetTickCount();
    deadline_ms = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;

    for (;;) {
        DWORD waited, remaining, n = 0, i;
        INPUT_RECORD recs[32];

        if (deadline_ms == INFINITE) {
            remaining = INFINITE;
        } else {
            waited = GetTickCount() - start;        /* wraps correctly */
            if (waited >= deadline_ms) return 0;
            remaining = deadline_ms - waited;
        }

        switch (WaitForSingleObject(h, remaining)) {
            case WAIT_OBJECT_0: break;
            case WAIT_TIMEOUT:  return 0;
            default:            return -1;
        }

        /* The handle is signalled by ANY input record, including key-UP and
         * focus events, which produce no bytes. Reporting readable on one of
         * those would send the caller into a read() that blocks past its
         * timeout. So look before answering.
         *
         * ANY key-down counts, not only one carrying a character. In virtual-
         * terminal input mode the console turns keys that have no character -
         * F1..F12, the arrows, Home/End/PgUp/PgDn - into ANSI escape sequences,
         * so those DO produce bytes for the read that follows. Testing
         * uChar.AsciiChar != 0 rejected exactly those keys, and the discard
         * below then ate them: F12 and the arrow keys were swallowed before
         * anything could see them.
         *
         * A key-down with a live modifier and no character (Shift alone, Ctrl
         * alone) still produces nothing; the discard handles those. */
        if (!PeekConsoleInput(h, recs, (DWORD)(sizeof recs / sizeof recs[0]), &n))
            return -1;
        if (n == 0) continue;

        for (i = 0; i < n; i++) {
            if (recs[i].EventType == KEY_EVENT && recs[i].Event.KeyEvent.bKeyDown)
                return 1;
        }

        /* Nothing readable in the queue - only key-ups and window events. Drop
         * ONE record so the wait cannot spin on the same event forever, then go
         * round again on what is left of the timeout.
         *
         * The scan above guarantees this never discards a key-down. */
        {
            INPUT_RECORD discard;
            DWORD got = 0;
            if (!ReadConsoleInput(h, &discard, 1, &got) || got == 0) return -1;
        }
    }
}

int nd_tty_read(void* buf, int max) {
    int n;
    if (!buf || max <= 0) return -1;
    n = _read(0, buf, (unsigned int)max);

    /* Translate CR to LF - the Windows stand-in for termios ICRNL.
     *
     * The POSIX side of this shim deliberately LEAVES ICRNL SET (see the note
     * in nd_tty_set_raw), so on Linux the guest receives 0x0A when Enter is
     * pressed. Windows has no such flag: in virtual-terminal input mode Enter
     * arrives as a bare 0x0D, and binary mode - which is required, or the CRT
     * mangles the stream in worse ways - hands that straight through.
     *
     * The guest therefore saw a different byte for the same key on the two
     * platforms, and NDIX's line discipline did not treat it as end of line:
     * measured as "Enter does nothing, it takes several presses". Doing the
     * translation here keeps raw mode meaning the same thing everywhere.
     *
     * Nothing else is affected: the escape sequences the function and cursor
     * keys produce contain no CR. */
    if (n > 0) {
        unsigned char* p = (unsigned char*)buf;
        int i;
        for (i = 0; i < n; i++) {
            if (p[i] == '\r') { p[i] = '\n'; continue; }

            /* Backspace: send DEL, not BS.
             *
             * NDIX's erase character is 0177 - kernel/MASTER/h/ttychars.h:
             * "#define CERASE 0177" - which is DEL (0x7F). A Unix terminal
             * sends exactly that for the Backspace key, so erase works there
             * without anyone thinking about it.
             *
             * The Windows console sends 0x08 (BS) instead. NDIX does not treat
             * that as erase, so Backspace did nothing at all: no character
             * removed, no cursor movement, because the guest never echoed the
             * erase sequence.
             *
             * Cost of the translation: Ctrl-H also produces 0x08 on this
             * console and becomes DEL too. The two keys are indistinguishable
             * once the console has turned them into bytes, and a working
             * Backspace is worth far more than a literal Ctrl-H. Ctrl-Backspace
             * still delivers a raw 0x7F if the guest ever needs it. */
            if (p[i] == 0x08) p[i] = 0x7F;
        }
    }
    return n;
}

void nd_tty_enable_ansi_output(void) {
    HANDLE h = tty_out();
    DWORD  mode;
    if (!is_console(h)) return;                /* redirected: nothing to set */
    if (!GetConsoleMode(h, &mode)) return;
    /* Failure is deliberately ignored: on a console too old to know this flag
     * the guest's escape codes show up as text, which is ugly but not fatal. */
    SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

/* ------------------------------------------------------------------------- */
/* POSIX                                                                      */
/* ------------------------------------------------------------------------- */
#else

int nd_tty_stdin_is_terminal(void) {
    return isatty(STDIN_FILENO) ? 1 : 0;
}

int nd_tty_save(nd_tty_mode* saved) {
    if (!saved) return -1;
    memset(saved, 0, sizeof *saved);
    if (!isatty(STDIN_FILENO)) return -1;
    if (tcgetattr(STDIN_FILENO, &saved->t) != 0) return -1;
    saved->valid = 1;
    return 0;
}

void nd_tty_restore(const nd_tty_mode* saved) {
    if (!saved || !saved->valid) return;
    tcsetattr(STDIN_FILENO, TCSANOW, &saved->t);
}

void nd_tty_flush_input(void) {
    if (isatty(STDIN_FILENO)) tcflush(STDIN_FILENO, TCIFLUSH);
}

int nd_tty_set_raw(const nd_tty_mode* base, int pass_signals) {
    struct termios raw;
    if (!base || !base->valid) return -1;
    raw = base->t;

    raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO);   /* no line buffer, no local echo */

    /* IEXTEN is the host's own extended input processing, and Ctrl-V (LNEXT) is
     * the part of it that runs even with ICANON off - the host would consume the
     * Ctrl-V and hand the guest only the quoted character. CLNEXT is Ctrl-V in
     * the guest too (h/ttychars.h:50), so the guest must receive the 0x16. */
    raw.c_lflag &= ~(tcflag_t)IEXTEN;

    /* Software flow control belongs to the GUEST, not to the host terminal.
     * With IXON left set (the default on a Linux tty) the host swallows Ctrl-S
     * and Ctrl-Q outright, so CSTOP/CSTART (h/ttychars.h:40-41) could never
     * reach NDIX - stopping and restarting guest output was impossible. IXANY
     * goes too, or any keystroke silently restarts output the guest still
     * believes it has stopped.
     *
     * Only the flags that STEAL characters are cleared. ICRNL deliberately
     * stays: Enter currently reaches the guest correctly, and cfmakeraw-style
     * blanket clearing would change that for no demonstrated gain. */
    raw.c_iflag &= ~(tcflag_t)(IXON | IXOFF | IXANY);

    /* Hand Ctrl-C (and Ctrl-\, Ctrl-Z) to the guest instead of letting the host
     * tty turn them into signals for the emulator. */
    if (!pass_signals) raw.c_lflag &= ~(tcflag_t)ISIG;

    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    return (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0) ? 0 : -1;
}

int nd_tty_wait_readable(int timeout_ms) {
    fd_set rf;
    struct timeval tv;
    int n;

    FD_ZERO(&rf);
    FD_SET(STDIN_FILENO, &rf);
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    n = select(STDIN_FILENO + 1, &rf, NULL, NULL, (timeout_ms < 0) ? NULL : &tv);
    if (n < 0) return (errno == EINTR) ? 0 : -1;   /* a signal is not an error */
    return n > 0 ? 1 : 0;
}

int nd_tty_read(void* buf, int max) {
    ssize_t got;
    if (!buf || max <= 0) return -1;
    got = read(STDIN_FILENO, buf, (size_t)max);
    return (int)got;
}

void nd_tty_enable_ansi_output(void) {
    /* POSIX terminals have always interpreted escape sequences. */
}

#endif
