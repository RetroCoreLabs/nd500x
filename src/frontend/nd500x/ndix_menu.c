/*
 * ndix_menu.c - the F12 menu. See ndix_menu.h.
 */

#include "ndix_menu.h"
#include "nd500x_ndix.h"
#include "../../cpu/nd500_fecall.h"
#include "../../machine/machine_types.h"

#include <stdio.h>
#include <stdarg.h>   /* row() takes a format and arguments */
#include <string.h>
#include <unistd.h>

/* F12 as terminals actually send it. xterm and the Linux console send
 * "\x1B[24~"; a few terminals send "\x1B[6~" - both are accepted, matching what
 * nd100x's keyboard.c already does, so one key works on both emulators. */
static const char* const F12_SEQS[] = { "\x1B[24~", "\x1B[6~" };

int ndix_menu_match_f12(const char* buf, int len) {
    size_t i;
    int partial = 0;

    if (len <= 0) return 0;
    for (i = 0; i < sizeof F12_SEQS / sizeof F12_SEQS[0]; i++) {
        const char* s = F12_SEQS[i];
        int slen = (int)strlen(s);
        if (len >= slen) {
            if (memcmp(buf, s, (size_t)slen) == 0) return slen;
        } else if (memcmp(buf, s, (size_t)len) == 0) {
            partial = 1;
        }
    }
    return partial ? -1 : 0;
}

/* The guest ttys this menu can attach the local terminal to.
 * Must stay in step with g_ttys[] in nd500x_ndix.c - the same lines, whether
 * they are reached over telnet or by switching this terminal.
 *
 * Local minors only. tty81 (minor 129) was here and is not any more: it is the
 * first REMOTE line, and io/mx.c:66 gives every minor from 129 up HARD carrier,
 * so its open() waits for the front end to report carrier the way a dial-in
 * line waits for DCD. That handshake does not yet complete, so the entry only
 * ever offered a terminal that stayed silent. */
static const struct { int unit; const char* name; } MENU_TTYS[] = {
    { 0, "console" },
    { 1, "tty01"   },
    { 2, "tty02"   },
    { 3, "tty03"   },
    { 4, "tty04"   },
    { 5, "tty05"   },
    { 6, "tty06"   },
    { 7, "tty07"   },
    { 8, "tty08"   },
};
#define MENU_TTY_COUNT ((int)(sizeof MENU_TTYS / sizeof MENU_TTYS[0]))

/* One keystroke, blocking. Returns -1 on EOF. stdin is already raw. */
static int read_key(void) {
    unsigned char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    return n == 1 ? (int)c : -1;
}

/* Version string for the menu header.
 *
 * ND500X_VERSION comes from the project() line in the top-level CMakeLists, so
 * there is one place to change it. The fallback only matters for a build that
 * bypasses CMake. __DATE__ and __TIME__ are stamped when THIS file is compiled,
 * which is what makes the header answer "am I running the binary I just built?"
 * - a question that came up repeatedly while the Windows build was being fixed,
 * because a running emulator holds its own .exe locked and an update silently
 * does not take. */
#ifndef ND500X_VERSION
#define ND500X_VERSION "unknown"
#endif
#define MENU_BUILD_LINE "v" ND500X_VERSION "  built " __DATE__ " " __TIME__

/* The menu box is drawn in exactly one width, and every row goes through the
 * two helpers below.
 *
 * Hand-padding each printf with its own run of spaces is what let the right-hand
 * border drift: the tty rows came out 43 wide and the "Telnet: ON" row 46, in a
 * 45-wide box. Formatting into a buffer and printing it with a single %-*s
 * makes every row the same width by construction, and a row that grows too long
 * is truncated rather than pushing the border out. */
/* Text columns between "| " and " |".
 *
 * Wide enough for the longest row that can occur, which is a tty line with
 * everything true at once:
 *
 *   " 1. console  (unit  0) operator <- local [255.255.255.255]"
 *    \__________/\________/\_______/\_______/\________________/
 *      4 + 8       10        9        9          18            = 58
 *
 * The box used to be 43, sized for the days when it held nothing but a name.
 * A real address on a LAN - 192.168.1.18 - ran off the end and was truncated
 * mid-number, which is worse than not showing it: a half-printed IP looks like
 * a different machine. 15 characters is the widest an IPv4 address gets
 * (255.255.255.255), and utmp names are up to 8, so this leaves both room. */
#define MENU_W 62

/* The border is DERIVED from MENU_W rather than written out as a literal row
 * of dashes. The two used to be independent, which is how the box came to be
 * drawn 45 wide while its rows were 43 and 46. */
static void rule(void) {
    int i;
    putchar('+');
    for (i = 0; i < MENU_W + 2; i++) putchar('-');
    printf("+\r\n");
}

/* One row of the box: "| <text padded to MENU_W> |". */
static void row(const char* fmt, ...) {
    char buf[MENU_W + 1];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    printf("| %-*s |\r\n", MENU_W, buf);
}

static void banner(const char* title) {
    printf("\r\n");
    rule();
    row("%s", title);
    rule();
}

/* Header with the build stamp under the title, for the top-level menu. */
static void banner_versioned(const char* title) {
    printf("\r\n");
    rule();
    row("%s", title);
    row("%s", MENU_BUILD_LINE);
    rule();
}

/* Who, if anyone, is on a given tty over telnet.
 *
 * The telnet server keeps its terminals in the same order as MENU_TTYS, so the
 * menu index is the server index. Returns a short tag for the line's state, and
 * fills <addr> with the client's IP:port when one is connected. */
static const char* telnet_state(int idx, char* addr, int addrlen) {
    const char* name = NULL;
    int connected = 0;

    if (addr && addrlen > 0) addr[0] = '\0';
    if (!nd500x_ndix_telnet_active()) return "";
    if (nd500x_ndix_telnet_info(idx, &name, &connected, addr, addrlen) != 0)
        return "";
    return connected ? "in use" : "free";
}

/* Sub-menu: pick which guest tty this terminal talks to, and manage the telnet
 * server - whether it is running, who is on which line, and hanging one up.
 *
 * Modelled on nd100x's terminal menu: ONE port serves every line, a client
 * chooses which when it connects, so the useful things to see here are which
 * lines are taken and by whom. */
static void console_menu(void) {
    int cur = nd500_fecall_local_unit();
    int i, k;

    for (;;) {
        int on   = nd500x_ndix_telnet_active();
        int port = nd500x_ndix_telnet_port();
        int pend = nd500x_ndix_telnet_pending();

        banner("Virtual consoles");
        for (i = 0; i < MENU_TTY_COUNT; i++) {
            char addr[64];
            const char* st = telnet_state(i, addr, (int)sizeof addr);
            char user[16];
            char note[64];
            size_t n;

            /* Three independent things can be true of one line: somebody may be
             * LOGGED IN on it, this window may be attached to it, and a telnet
             * client may hold it. Show all three - being logged in and being
             * connected are not the same thing, and the difference is exactly
             * what you want to see before disconnecting somebody. */
            note[0] = '\0';
            n = 0;

            /* 8 columns: utmp's ut_name field is 8 wide, so that is the widest
             * a user name can be. */
            if (nd500x_ndix_utmp_user(MENU_TTYS[i].name, user, (int)sizeof user) == 0)
                n += (size_t)snprintf(note + n, sizeof note - n, " %-8s", user);
            else
                n += (size_t)snprintf(note + n, sizeof note - n, " %-8s", "-");

            if (MENU_TTYS[i].unit == cur)
                n += (size_t)snprintf(note + n, sizeof note - n, " <- local");
            if (st[0] && strcmp(st, "in use") == 0) {
                /* Host only, no port: "root [127.0.0.1:34540]" ran past the
                 * right border and was truncated mid-address, which is worse
                 * than showing less. The full address with its port is on the
                 * disconnect list, where the row is short enough to carry it. */
                char host[32];
                const char* colon;
                snprintf(host, sizeof host, "%s", addr[0] ? addr : "in use");
                colon = strrchr(host, ':');
                if (colon) host[colon - host] = '\0';
                snprintf(note + n, sizeof note - n, " [%s]", host);
            }

            row(" %d. %-8s (unit %2d)%s", i + 1, MENU_TTYS[i].name,
                MENU_TTYS[i].unit, note);
        }
        rule();
        if (on)
            row(" T. Telnet: ON, port %d%s", port,
                pend ? "  (client at menu)" : "");
        else
            row(" T. Telnet: OFF - press T to start it");
        row(" D. Disconnect a telnet client");
        row(" 0. Resume NDIX");
        rule();
        printf("choice: ");
        fflush(stdout);

        k = read_key();
        printf("\r\n");
        if (k < 0 || k == '0' || k == 27) return;

        if (k == 't' || k == 'T') {
            if (on) {
                nd500x_ndix_telnet_stop();
                printf("[menu] telnet server stopped - connected clients were "
                       "dropped; the guest lines stay logged in\r\n");
            } else {
                /* 0 = every terminal in the table. The port is the same default
                 * --telnet uses, so starting it here and starting it on the
                 * command line land in the same place. */
                if (nd500x_ndix_telnet_start(5000, 0) == 0) {
                    /* Claim the line this window is on straight away, or the
                     * first client to connect would be offered it. */
                    nd500x_ndix_telnet_mark_local(cur);
                    printf("[menu] telnet server started - connect with: "
                           "telnet localhost 5000\r\n");
                }
                else
                    printf("[menu] could not start the telnet server (port in "
                           "use?)\r\n");
            }
            fflush(stdout);
            continue;                     /* redraw, so the new state shows */
        }

        if (k == 'd' || k == 'D') {
            int nconn = 0;

            if (!nd500x_ndix_telnet_active()) {
                printf("[menu] the telnet server is not running - press T to "
                       "start it\r\n");
                fflush(stdout);
                continue;
            }

            /* Show only the lines that actually have a client, and say plainly
             * when there are none. Asking "which terminal (1-9)?" when nobody
             * is connected invites picking one and being told it had no client
             * - a question that should never have been asked. */
            for (i = 0; i < MENU_TTY_COUNT; i++)
                if (nd500x_ndix_telnet_in_use(i)) nconn++;

            if (nconn == 0) {
                printf("[menu] nobody is connected - nothing to disconnect\r\n");
                fflush(stdout);
                continue;
            }

            banner("Connected telnet clients");
            for (i = 0; i < MENU_TTY_COUNT; i++) {
                char addr[64];
                char user[16];
                const char* name = NULL;
                int conn = 0;
                if (!nd500x_ndix_telnet_in_use(i)) continue;
                nd500x_ndix_telnet_info(i, &name, &conn, addr, (int)sizeof addr);
                if (nd500x_ndix_utmp_user(MENU_TTYS[i].name, user, (int)sizeof user) != 0)
                    snprintf(user, sizeof user, "-");
                /* Same numbers as the terminal list above, so a line keeps one
                 * number wherever it is shown. Who is logged in matters here:
                 * disconnecting also logs them out. */
                row(" %d. %-8s %-8s %s", i + 1, MENU_TTYS[i].name, user,
                    addr[0] ? addr : "(client attached)");
            }
            row(" 0. Cancel");
            rule();
            printf("disconnect which? ");
            fflush(stdout);

            k = read_key();
            printf("\r\n");
            if (k >= '1' && k < '1' + MENU_TTY_COUNT) {
                int idx = k - '1';
                if (!nd500x_ndix_telnet_in_use(idx))
                    printf("[menu] %s has no telnet client\r\n", MENU_TTYS[idx].name);
                else if (nd500x_ndix_telnet_disconnect(idx) == 0)
                    printf("[menu] %s disconnected and logged out - it will "
                           "come back at a login prompt\r\n", MENU_TTYS[idx].name);
                else
                    printf("[menu] could not disconnect %s\r\n", MENU_TTYS[idx].name);
            }
            fflush(stdout);
            continue;
        }

        if (k >= '1' && k < '1' + MENU_TTY_COUNT) {
            int idx = k - '1';

            /* Only a FREE line may be taken. Two readers on one tty means each
             * sees half the keystrokes and both see all the output, which looks
             * like the terminal breaking rather than like a conflict. The same
             * rule applies the other way round: nd500x_ndix_telnet_mark_local()
             * below stops the telnet menu offering whichever line this window
             * holds. */
            if (nd500x_ndix_telnet_in_use(idx)) {
                char addr[64];
                const char* name = NULL;
                int conn = 0;
                nd500x_ndix_telnet_info(idx, &name, &conn, addr, (int)sizeof addr);
                printf("[menu] %s is in use by a telnet client%s%s - "
                       "disconnect it first with D\r\n",
                       MENU_TTYS[idx].name, addr[0] ? " at " : "",
                       addr[0] ? addr : "");
                fflush(stdout);
                continue;
            }

            nd500_fecall_set_local_unit(MENU_TTYS[idx].unit);
            cur = MENU_TTYS[idx].unit;
            /* Hand the old line back and claim the new one. */
            nd500x_ndix_telnet_mark_local(cur);
            /* Say what happened AND that the screen is not redrawn: this
             * terminal now shows only what the new tty prints from here on, and
             * a tty whose getty already printed its prompt will look silent
             * until it writes again. Pressing Enter is the usual way to make it
             * speak. */
            printf("[menu] terminal attached to %s (unit %d) - press Enter if it "
                   "looks silent\r\n", MENU_TTYS[idx].name, MENU_TTYS[idx].unit);
            fflush(stdout);
            return;
        }

        printf("[menu] no such choice\r\n");
        fflush(stdout);
    }
}

/* The debugger's hot-key handler: see Nd500GuestKeyFn in debugger.h for what
 * the return values mean. */
int ndix_menu_guest_key(struct Nd500Machine* m, const char* buf, int len) {
    int n = ndix_menu_match_f12(buf, len);
    if (n == 0) return 0;          /* not F12 - the guest gets these bytes */
    if (n < 0)  return -2;         /* prefix only - the caller reads more */
    if (ndix_menu_run(m) == NDIX_MENU_QUIT) return -1;
    return n;
}

NdixMenuResult ndix_menu_run(struct Nd500Machine* m) {
    int k;

    banner_versioned("nd500x - NDIX");
    row(" 1. Virtual consoles");
    row(" 2. Shut down NDIX (sync, then halt)");
    row(" 3. Exit now, WITHOUT syncing");
    row(" 0. Resume NDIX");
    rule();
    printf("choice: ");
    fflush(stdout);

    k = read_key();
    printf("\r\n");

    switch (k) {
    case '1':
        console_menu();
        return NDIX_MENU_RESUME;
    case '2':
        /* The clean route. We do NOT stop the CPU here - we hand the guest to
         * the kernel's own boot() halt path and keep running it, because the
         * flush only happens while the CPU is still executing. The machine
         * stops when boot() reaches feexit_fecall(), which arrives as FE_EXIT
         * and clears run_flag there.
         *
         * So RESUME, not QUIT, is the correct return here even though the
         * machine is on its way down.
         *
         * If the guest is wedged and never gets there, nothing is lost: press
         * F12 again and take option 3. That is exactly why both entries exist
         * rather than one that tries to be clever. */
        printf("[menu] shutting NDIX down - syncing disks\r\n");
        fflush(stdout);
        if (nd500x_ndix_halt_guest(m) != 0) {
            printf("[menu] clean shutdown unavailable; use 3 to exit without "
                   "syncing (the image will need fsck)\r\n");
            fflush(stdout);
        }
        return NDIX_MENU_RESUME;
    case '3':
        /* The escape hatch, and the old behaviour of option 2. Stops the CPU
         * where it stands, so any buffer the guest had not written is lost and
         * the filesystem is left dirty - "/etc/fsck -y /dev/rdi0a" repairs it.
         * Still unwinds through the caller rather than exiting on the spot, so
         * the telnet server is stopped and the image is closed properly. */
        printf("[menu] exiting WITHOUT syncing - the image will need fsck\r\n");
        fflush(stdout);
        if (m) m->run_flag = 0;
        return NDIX_MENU_QUIT;
    default:
        return NDIX_MENU_RESUME;
    }
}
