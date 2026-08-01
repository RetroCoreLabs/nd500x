/*
 * nd500x_ndix.c - NDIX boot setup (--ndix) and the guest-tty telnet bridge.
 *
 * See nd500x_ndix.h. Two independent pieces live here because both exist only
 * to serve the NDIX guest:
 *
 *   1. nd500x_ndix_setup() - everything the run-ndix.sh wrapper used to do.
 *   2. the telnet bridge   - maps a registered terminal to an NDIX mx unit.
 *
 * NDIX terminal model (io/mx.c): the fecall device word is (generic << 16) |
 * unit; generic 3 = TERM_IN, generic 4 = TERM_OUT. Unit 0 is /dev/console.
 * Input for every unit shares one guest ring whose elements are
 * (unit << 8) | char, which is why nd500_fecall_tty_input() takes the unit.
 */

#include "nd500x_ndix.h"
#include "telnetserver.h"
#include "../../cpu/nd500_fecall.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <sys/stat.h>

#include <ndmon/mon_config.h>

/* Set by nd500x_ndix_setup: the kernel path, and whether no <kernel>.init exists
 * so we have to do the boot setup ourselves. */
static char g_auto_kernel[PATH_MAX];
static int  g_auto_boot;


/* ------------------------------------------------------------ boot setup -- */

/* setenv only if the caller has not already chosen a value: --ndix supplies
 * DEFAULTS, and every one of these variables must stay usable as an override. */
static void setenv_default(const char* name, const char* value) {
    const char* cur = getenv(name);
    if (cur && cur[0]) return;
    setenv(name, value, 1);
}

static int is_file(const char* p) {
    struct stat st;
    return p && p[0] && stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

int nd500x_ndix_setup(const char* image, const char* kernel, const char* root_opt,
                      char* load_cmd, int load_cmd_len) {
    char abs_image[PATH_MAX];
    char root[PATH_MAX];
    char kern[PATH_MAX];
    char tmp[PATH_MAX];

    if (!image || !image[0]) {
        fprintf(stderr, "error: --ndix needs a root disk image path\n");
        return -1;
    }
    if (!realpath(image, abs_image)) {
        fprintf(stderr, "error: --ndix disk image not found: %s\n", image);
        return -1;
    }
    if (!is_file(abs_image)) {
        fprintf(stderr, "error: --ndix disk image is not a regular file: %s\n", abs_image);
        return -1;
    }

    /* SINTRAN root: an explicit --sintran-root wins, else the directory the
     * disk image lives in (that is where the NDIX tree is rooted). */
    if (root_opt && root_opt[0]) {
        if (!realpath(root_opt, root)) {
            fprintf(stderr, "error: --sintran-root not found: %s\n", root_opt);
            return -1;
        }
    } else {
        snprintf(tmp, sizeof tmp, "%s", abs_image);
        snprintf(root, sizeof root, "%s", dirname(tmp));
    }

    /* Kernel image, in order of decreasing explicitness. The disk image and
     * the kernel are separate things, so the kernel is never assumed to be
     * next to the image without being checked for. */
    kern[0] = '\0';
    if (kernel && kernel[0]) {
        if (!realpath(kernel, kern)) {
            fprintf(stderr, "error: --kernel not found: %s\n", kernel);
            return -1;
        }
    } else {
        const char* env = getenv("ND500X_KERNEL");
        const char* rel[] = { "kernel/MASTER/GENERIC/vmunix", "vmunix" };
        if (env && env[0]) {
            if (!realpath(env, kern)) {
                fprintf(stderr, "error: ND500X_KERNEL not found: %s\n", env);
                return -1;
            }
        } else {
            char cand[PATH_MAX + 64];
            size_t i;
            for (i = 0; i < sizeof rel / sizeof rel[0]; i++) {
                snprintf(cand, sizeof cand, "%s/%s", root, rel[i]);
                if (is_file(cand) && realpath(cand, kern)) break;
                kern[0] = '\0';
            }
            if (!kern[0]) {
                fprintf(stderr,
                        "error: no NDIX kernel found. Looked for:\n"
                        "         %s/kernel/MASTER/GENERIC/vmunix\n"
                        "         %s/vmunix\n"
                        "       Give it explicitly with --kernel <path> or "
                        "ND500X_KERNEL=<path>.\n", root, root);
                return -1;
            }
        }
    }
    if (!is_file(kern)) {
        fprintf(stderr, "error: kernel image is not a regular file: %s\n", kern);
        return -1;
    }

    /* Environment defaults. Each is REQUIRED for the NDIX boot and each stays
     * overridable by exporting it before launch:
     *   ND500X_DISK             root disk image the fecall disk device serves
     *   ND500X_MMU_GUEST_TABLES route translation through the guest MMU tables
     *   ND500X_NOXMSG           bypass XMSG (without it proc0 sleeps forever)
     *   ND500X_DISK_RW          copy-on-write session file; master stays clean
     *   ND500X_CONSOLE_STDIN    typed lines go to the guest console, not the
     *                           debugger ('~' prefix reaches the debugger) */
    setenv_default("ND500X_DISK", abs_image);
    setenv_default("ND500X_MMU_GUEST_TABLES", "1");
    setenv_default("ND500X_NOXMSG", "1");
    setenv_default("ND500X_DISK_RW", "1");
    setenv_default("ND500X_CONSOLE_STDIN", "1");

    mon_config_set_sintran_root(root);

    /* The debugger's `load` resolves its argument against the CURRENT working
     * directory and auto-sources <name>.init from the same place, so run from
     * the kernel directory and load by bare name. Doing it here is what frees
     * the user from having to cd there first. */
    snprintf(tmp, sizeof tmp, "%s", kern);
    char* kdir = dirname(tmp);
    if (chdir(kdir) != 0) {
        fprintf(stderr, "error: cannot chdir to kernel directory %s\n", kdir);
        return -1;
    }

    char base[PATH_MAX];
    snprintf(base, sizeof base, "%s", kern);

    /* An <kernel>.init next to the kernel WINS: `load` auto-sources it, and any
     * existing setup keeps working untouched. Only when there is none do we boot
     * the kernel ourselves from values derived off the files (see
     * nd500x_ndix_autoboot). */
    snprintf(g_auto_kernel, sizeof g_auto_kernel, "%s", kern);
    {
        char initf[PATH_MAX];
        snprintf(initf, sizeof initf, "%s.init", basename(base));
        g_auto_boot = is_file(initf) ? 0 : 1;
    }
    snprintf(load_cmd, (size_t)load_cmd_len, "load %s", basename(base));

    fprintf(stderr, "[ndix] disk   : %s\n", abs_image);
    fprintf(stderr, "[ndix] root   : %s\n", root);
    fprintf(stderr, "[ndix] kernel : %s\n", kern);
    return 0;
}

/* ---------------------------------------------------------- auto boot ------ */
int nd500x_ndix_autoboot_needed(void) { return g_auto_boot; }

/* Boot the kernel without an .init file.
 *
 * Everything the old vmunix.init hand-wrote is derived from the files here, so a
 * rebuilt kernel cannot silently desync:
 *   - the .pseg / .dseg paths come from the --kernel path
 *   - the .dseg load address is the .pseg size rounded to a 2 KB page
 *   - map-kdata gets that same address and the real .dseg size
 * (verified against the shipped kernel: pseg 0x42000, dseg 0x3E800, matching the
 * 0x42000 / 0x3E800 constants the init file carried by hand).
 *
 * THA/CTE1/CTE2/CAD stand in for what SINTRAN's context load would have set. */
int nd500x_ndix_autoboot(struct Nd500Machine* m,
                         int (*run)(struct Nd500Machine*, const char*, void*),
                         void* ctx) {
    char pseg[PATH_MAX], dseg[PATH_MAX], cmd[PATH_MAX + 64];
    struct stat sp, sd;

    snprintf(pseg, sizeof pseg, "%s.pseg", g_auto_kernel);
    snprintf(dseg, sizeof dseg, "%s.dseg", g_auto_kernel);
    if (stat(pseg, &sp) != 0 || stat(dseg, &sd) != 0) {
        fprintf(stderr, "error: --ndix needs %s and %s beside the kernel\n", pseg, dseg);
        return -1;
    }

    /* .dseg follows .pseg, page-aligned (NBPG = 2048). */
    unsigned long dseg_load = ((unsigned long)sp.st_size + 0x7FFUL) & ~0x7FFUL;
    unsigned long dseg_size = (unsigned long)sd.st_size;

    fprintf(stderr, "[ndix] auto-boot: pseg=%lu dseg=%lu -> dseg@0x%08lX kdata 0x%08lX+0x%lX\n",
            (unsigned long)sp.st_size, dseg_size, dseg_load, dseg_load, dseg_size);

    /* `load` first: it reads the a.out and SETS THE ENTRY PC from the header.
     * The normal path gets this because the debugger's `load` auto-sources
     * <name>.init AFTER loading; here there is no .init, so we issue the same
     * load and then do by hand what that .init would have done. Without this the
     * machine starts at PC=0 and immediately stops on an invalid instruction. */
    {
        char kbase[PATH_MAX];
        snprintf(kbase, sizeof kbase, "%s", g_auto_kernel);
        snprintf(cmd, sizeof cmd, "load %s", basename(kbase));
        run(m, cmd, ctx);
    }

    run(m, "mmusetup", ctx);
    snprintf(cmd, sizeof cmd, "load-pseg %s 0x00000000", pseg);          run(m, cmd, ctx);
    snprintf(cmd, sizeof cmd, "load-dseg %s 0x%08lX", dseg, dseg_load);  run(m, cmd, ctx);
    snprintf(cmd, sizeof cmd, "map-kdata 0x%08lX 0x%08lX", dseg_load, dseg_size);
    run(m, cmd, ctx);
    /* THA = _u + U_CXB0, NOT _u + _Ktrap.
     *
     * The runtime trap vector is set LIVE by the kernel's __resume
     * (machine/locore.c:964-966): tha := _u + U_CXB0 + traplev*496, and
     * U_CXB0 = 1852 = 0x73C (machine/locore.h:66). At traplev 0 that is
     * 0xE800073C. The static _Ktrap = _u+0x736 (locore.c:183), which
     * kpcbinit stores in pcb_tha, is 6 bytes LOWER and is not 4-byte aligned;
     * cpu.c:942-949 already documents that it yields a misaligned vector.
     *
     * Measured: with 0xE8000736 a trap 38 reads its slot at 0xE80007CE, so the
     * big-endian word straddles THA[36]=0x00000365 and THA[37]=0x00000373 and
     * returns 0x03650000 - a garbage handler address. With 0xE800073C the same
     * slot is 0xE80007D4 and holds the real page-fault handler 0x00000381.
     *
     * This bootstrap value only matters for a trap raised in domain 0 before
     * the first __resume/domain switch, since a switch reloads THA. A healthy
     * boot never traps that early, which is why the wrong value went unnoticed;
     * a memory-starved boot does, and died here. */
    run(m, "set THA 0xE800073C", ctx);
    run(m, "set CTE1 0xF413D800", ctx);
    run(m, "set CTE2 0x0000005F", ctx);
    run(m, "set CAD 1", ctx);
    return 0;
}

/* ---------------------------------------------------------- telnet bridge -- */

/* One record per guest tty offered over telnet. Its address doubles as the
 * opaque struct Device* key the terminal server uses. */
typedef struct NdixTty {
    int  unit;              /* NDIX mx unit (device word low half) */
    char name[32];
    int  last_out;          /* last byte sent, for bare-LF expansion */
    int  swallow;           /* drop a LF/NUL that pairs with a just-seen CR */
} NdixTty;

/* Units mirror the shipped image's /dev entries: console (major 0 minor 0)
 * plus tty01/tty02/tty81 (minors 1, 2, 129). Only the console has a getty in
 * the shipped /etc/ttys; the others are offered so that enabling one is a
 * guest-side change, not an emulator change. */
static NdixTty g_ttys[] = {
    { 0,   "console", 0, 0 },
    { 1,   "tty01",   0, 0 },
    { 2,   "tty02",   0, 0 },
    { 129, "tty81",   0, 0 },
};
#define NDIX_TTY_COUNT ((int)(sizeof g_ttys / sizeof g_ttys[0]))

static TelnetServer* g_server = NULL;

/* Client -> guest. The server hands us raw bytes (rawInput), so the CR/LF
 * normalisation happens here: a terminal's Enter key is CR, possibly followed
 * by the LF or NUL a telnet client pairs with it, and the NDIX console input
 * path is driven with LF (that is what the stdin console path feeds it). */
static void ndix_tty_in(struct Device* dev, uint8_t keycode) {
    NdixTty* t = (NdixTty*)dev;
    char c = (char)keycode;
    if (!t) return;
    if (keycode == '\r') {
        t->swallow = 1;
        c = '\n';
    } else if (keycode == '\n' || keycode == 0) {
        if (t->swallow) { t->swallow = 0; return; }
        if (keycode == 0) return;            /* stray NUL padding */
    } else {
        t->swallow = 0;
    }
    nd500_fecall_tty_input(t->unit, &c, 1);
}

/* Guest -> client. Bytes arrive already masked by the fecall console path. */
static void ndix_tty_out(int unit, const unsigned char* buf, int len, void* ctx) {
    NdixTty* t = (NdixTty*)ctx;
    int i;
    (void)unit;
    if (!t) return;
    for (i = 0; i < len; i++) {
        unsigned char c = buf[i];
        if (c == 0) continue;                /* fill/padding, nothing to show */
        if (c == '\n' && t->last_out != '\r')
            telnet_output_handler((struct Device*)t, '\r');
        telnet_output_handler((struct Device*)t, (char)c);
        t->last_out = c;
    }
}

int nd500x_ndix_telnet_start(int port) {
    TelnetServerConfig cfg;
    int i;

    if (g_server) return 0;
    memset(&cfg, 0, sizeof cfg);
    cfg.port = port;
    cfg.maxConnections = NDIX_TTY_COUNT;
    cfg.transport = TRANSPORT_TELNET;

    g_server = TelnetServer_Create(&cfg);
    if (!g_server) {
        fprintf(stderr, "[telnet] cannot create server\n");
        return -1;
    }

    for (i = 0; i < NDIX_TTY_COUNT; i++) {
        TelnetTerminalInfo info;
        memset(&info, 0, sizeof info);
        info.device    = (struct Device*)&g_ttys[i];
        info.identCode = (uint16_t)g_ttys[i].unit;
        info.ioAddress = 0;
        info.name      = g_ttys[i].name;
        info.inputFunc = ndix_tty_in;
        info.rawInput  = true;
        if (!TelnetServer_RegisterTerminal(g_server, &info)) {
            fprintf(stderr, "[telnet] cannot register terminal %s\n", g_ttys[i].name);
            TelnetServer_Destroy(g_server);
            g_server = NULL;
            return -1;
        }
        nd500_fecall_set_tty_output(g_ttys[i].unit, ndix_tty_out, &g_ttys[i]);
    }

    if (!TelnetServer_Start(g_server)) {
        fprintf(stderr, "[telnet] cannot start server on port %d\n", port);
        for (i = 0; i < NDIX_TTY_COUNT; i++)
            nd500_fecall_set_tty_output(g_ttys[i].unit, NULL, NULL);
        TelnetServer_Destroy(g_server);
        g_server = NULL;
        return -1;
    }
    fprintf(stderr, "[telnet] guest terminals on port %d - connect with a telnet "
                    "client to localhost %d\n", port, port);
    return 0;
}

void nd500x_ndix_telnet_stop(void) {
    int i;
    if (!g_server) return;
    for (i = 0; i < NDIX_TTY_COUNT; i++)
        nd500_fecall_set_tty_output(g_ttys[i].unit, NULL, NULL);
    TelnetServer_Stop(g_server);
    TelnetServer_Destroy(g_server);
    g_server = NULL;
}

int nd500x_ndix_telnet_active(void) { return g_server != NULL; }
