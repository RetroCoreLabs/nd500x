/*
 * nd500_host_posix.c - the default host services: stdio-backed disk images.
 *
 * This is what nd500_fecall.c used to do inline. Moving it here changes no
 * behaviour on a desktop build - the same environment variables, the same
 * open modes, the same messages - but it makes the behaviour REPLACEABLE, which
 * is the whole point: a WASM build has neither getenv() nor fopen(), and a host
 * that already owns its storage (nd100x serves disk blocks out of OPFS or over
 * its gateway WebSocket) needs to answer for itself.
 *
 * See nd500_host.h for the interface and the reasoning.
 */

#include "nd500_host.h"
#include "nd500_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* ---------------------------------------------------------------------------
 * Units
 * ------------------------------------------------------------------------- */

typedef struct {
    FILE*   f;
    int64_t size;
    int     writable;      /* 1 = honour guest writes */
    char    path[1024];
} PosixUnit;

static PosixUnit s_unit[ND500_HOST_MAX_DISKS];

/* Unit 0 is auto-mounted from ND500X_DISK the first time anybody asks about it,
 * which is what kept "nd500x --ndix <image>" working with no explicit mount.
 * Tried once: a failure must not be retried on every single fecall. */
static int s_autotried = 0;

static int posix_debug(void) {
    return nd500_settings()->fedbg;
}

/*
 * Mount an image on a unit.
 *
 * writable: 0 = read-only, 1 = write straight through to the image,
 *           2 = copy-on-write session.
 *
 * The COW mode copies the image to "<path>.session" and opens the COPY r+b, so
 * reads AND writes go to the copy and the master is never touched. Promote a
 * good session by copying it over the master by hand.
 */
int nd500_host_posix_mount(int unit, const char* path, int writable) {
    if (unit < 0 || unit >= ND500_HOST_MAX_DISKS || !path || !path[0]) return -1;
    PosixUnit* u = &s_unit[unit];

    if (u->f) { fclose(u->f); u->f = NULL; u->size = 0; u->writable = 0; }

    snprintf(u->path, sizeof(u->path), "%s", path);
    u->writable = (writable != 0);

    if (writable == 2) {
        char spath[1100];
        snprintf(spath, sizeof(spath), "%s.session", path);
        FILE* src = fopen(path, "rb");
        FILE* dst = src ? fopen(spath, "wb") : NULL;
        if (src && dst) {
            char buf[65536]; size_t n;
            while ((n = fread(buf, 1, sizeof(buf), src)) > 0) fwrite(buf, 1, n, dst);
        }
        if (src) fclose(src);
        if (dst) fclose(dst);
        u->f = dst ? fopen(spath, "r+b") : NULL;
        if (posix_debug() || u->f)
            fprintf(stderr, "[FECALL] COW session: %s -> %s (%s)\n",
                    path, spath, u->f ? "writable" : "FAILED - no disk");
    } else if (writable == 1) {
        u->f = fopen(path, "r+b");
        if (!u->f) {
            /* Say so rather than falling back silently: a read-only open looks
             * like a working boot right up to the first write, which then
             * vanishes with no error anywhere. */
            fprintf(stderr, "[FECALL] cannot open %s for writing (%s) - "
                            "opening read-only, guest writes will be LOST\n",
                    path, strerror(errno));
            u->f = fopen(path, "rb");
            u->writable = 0;
        }
    } else {
        u->f = fopen(path, "rb");
    }

    if (u->f) {
        fseek(u->f, 0, SEEK_END);
        u->size = (int64_t)ftell(u->f);
        fseek(u->f, 0, SEEK_SET);
    } else {
        u->size = 0;
    }

    if (posix_debug())
        fprintf(stderr, "[FECALL] disk image: %s (%lld bytes)\n",
                path, u->f ? (long long)u->size : -1LL);

    return u->f ? 0 : -1;
}

void nd500_host_posix_unmount_all(void) {
    for (int i = 0; i < ND500_HOST_MAX_DISKS; i++) {
        if (s_unit[i].f) fclose(s_unit[i].f);
        s_unit[i].f = NULL;
        s_unit[i].size = 0;
        s_unit[i].writable = 0;
        s_unit[i].path[0] = '\0';
    }
    s_autotried = 0;
}

/* Auto-mount unit 0 from the settings struct. The variable names and their
 * meanings live in one place now - nd500_settings.c - so this just uses what
 * it is told. */
static void posix_autopmount_unit0(void) {
    if (s_autotried || s_unit[0].f) return;
    s_autotried = 1;

    const Nd500Settings* cfg = nd500_settings();
    if (!cfg->disk_path || !cfg->disk_path[0]) return;   /* caller reports it */

    nd500_host_posix_mount(0, cfg->disk_path, cfg->disk_mode);
}

/* ---------------------------------------------------------------------------
 * The ops
 * ------------------------------------------------------------------------- */

static PosixUnit* unit_of(int unit) {
    if (unit < 0 || unit >= ND500_HOST_MAX_DISKS) return 0;
    if (unit == 0) posix_autopmount_unit0();
    return s_unit[unit].f ? &s_unit[unit] : 0;
}

static int64_t posix_disk_size(void* ctx, int unit) {
    (void)ctx;
    PosixUnit* u = unit_of(unit);
    return u ? u->size : -1;
}

static int64_t posix_disk_read(void* ctx, int unit, uint64_t off, void* dst, uint32_t len) {
    (void)ctx;
    PosixUnit* u = unit_of(unit);
    if (!u) return -1;
    if (fseek(u->f, (long)off, SEEK_SET) != 0) return -1;
    size_t got = fread(dst, 1, len, u->f);
    return (int64_t)got;
}

static int64_t posix_disk_write(void* ctx, int unit, uint64_t off, const void* src, uint32_t len) {
    (void)ctx;
    PosixUnit* u = unit_of(unit);
    if (!u || !u->writable) return -1;
    if (fseek(u->f, (long)off, SEEK_SET) != 0) return -1;
    size_t put = fwrite(src, 1, len, u->f);
    fflush(u->f);
    return (int64_t)put;
}

static int posix_disk_writable(void* ctx, int unit) {
    (void)ctx;
    PosixUnit* u = unit_of(unit);
    return u ? u->writable : 0;
}

static const Nd500HostOps s_posix_ops = {
    posix_disk_size,
    posix_disk_read,
    posix_disk_write,
    posix_disk_writable
};

const Nd500HostOps* nd500_host_posix_ops(void) { return &s_posix_ops; }

/* ---------------------------------------------------------------------------
 * Installation and the convenience wrappers
 * ------------------------------------------------------------------------- */

static const Nd500HostOps* s_ops = 0;
static void*               s_ctx = 0;

void nd500_host_set(const Nd500HostOps* ops, void* ctx) {
    s_ops = ops ? ops : &s_posix_ops;
    s_ctx = ops ? ctx : 0;
}

const Nd500HostOps* nd500_host(void) {
    if (!s_ops) nd500_host_set(0, 0);   /* default to POSIX on first use */
    return s_ops;
}

void* nd500_host_ctx(void) {
    if (!s_ops) nd500_host_set(0, 0);
    return s_ctx;
}

int64_t nd500_host_disk_size(int unit) {
    const Nd500HostOps* o = nd500_host();
    return o->disk_size ? o->disk_size(nd500_host_ctx(), unit) : -1;
}

int64_t nd500_host_disk_read(int unit, uint64_t off, void* dst, uint32_t len) {
    const Nd500HostOps* o = nd500_host();
    return o->disk_read ? o->disk_read(nd500_host_ctx(), unit, off, dst, len) : -1;
}

int64_t nd500_host_disk_write(int unit, uint64_t off, const void* src, uint32_t len) {
    const Nd500HostOps* o = nd500_host();
    return o->disk_write ? o->disk_write(nd500_host_ctx(), unit, off, src, len) : -1;
}

int nd500_host_disk_writable(int unit) {
    const Nd500HostOps* o = nd500_host();
    return o->disk_writable ? o->disk_writable(nd500_host_ctx(), unit) : 0;
}
