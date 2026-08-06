/*
 * ndix_menu.c - the F12 menu. See ndix_menu.h.
 */

#include "ndix_menu.h"
#include "nd500x_ndix.h"
#include "../../cpu/nd500_fecall.h"
#include "../../machine/machine_types.h"

#include <stdio.h>
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

/* The guest ttys this menu can attach the local terminal to. Same four units
 * and the same order as the telnet bridge in nd500x_ndix.c, because they are
 * the same terminals - these are the /dev entries the image actually has
 * (console 0/0, tty01 0/1, tty02 0/2, tty81 0/129). */
static const struct { int unit; const char* name; } MENU_TTYS[] = {
    { 0,   "console" },
    { 1,   "tty01"   },
    { 2,   "tty02"   },
    { 129, "tty81"   },
};
#define MENU_TTY_COUNT ((int)(sizeof MENU_TTYS / sizeof MENU_TTYS[0]))

/* One keystroke, blocking. Returns -1 on EOF. stdin is already raw. */
static int read_key(void) {
    unsigned char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    return n == 1 ? (int)c : -1;
}

static void banner(const char* title) {
    printf("\r\n");
    printf("+---------------------------------------------+\r\n");
    printf("| %-43s |\r\n", title);
    printf("+---------------------------------------------+\r\n");
}

/* Sub-menu: pick which guest tty this terminal talks to. */
static void console_menu(void) {
    int cur = nd500_fecall_local_unit();
    int i, k;

    banner("Virtual consoles");
    for (i = 0; i < MENU_TTY_COUNT; i++)
        printf("|  %d. %-8s (unit %3d)%-19s|\r\n", i + 1, MENU_TTYS[i].name,
               MENU_TTYS[i].unit,
               MENU_TTYS[i].unit == cur ? "  <- attached" : "");
    printf("|  0. Resume NDIX                             |\r\n");
    printf("+---------------------------------------------+\r\n");
    printf("choice: ");
    fflush(stdout);

    k = read_key();
    printf("\r\n");
    if (k < 0 || k == '0' || k == 27) return;
    if (k >= '1' && k < '1' + MENU_TTY_COUNT) {
        int idx = k - '1';
        nd500_fecall_set_local_unit(MENU_TTYS[idx].unit);
        /* Say what happened AND that the screen is not redrawn: this terminal
         * now shows only what the new tty prints from here on, and a tty whose
         * getty already printed its prompt will look silent until it writes
         * again. Pressing Enter is the usual way to make it speak. */
        printf("[menu] terminal attached to %s (unit %d) - press Enter if it "
               "looks silent\r\n", MENU_TTYS[idx].name, MENU_TTYS[idx].unit);
    } else {
        printf("[menu] no such choice\r\n");
    }
    fflush(stdout);
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

    banner("nd500x - NDIX");
    printf("|  1. Virtual consoles                        |\r\n");
    printf("|  2. Shut down NDIX (sync, then halt)        |\r\n");
    printf("|  3. Exit now, WITHOUT syncing               |\r\n");
    printf("|  0. Resume NDIX                             |\r\n");
    printf("+---------------------------------------------+\r\n");
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
