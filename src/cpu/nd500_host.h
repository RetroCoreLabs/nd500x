/*
 * nd500_host.h - what the ND-500 needs FROM ITS HOST PROCESS.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * WHY THIS EXISTS
 * ---------------
 * NDIX has no I/O of its own. Every device it owns - disk (io/di.c), tape
 * (io/ct.c, io/mt.c), terminals (io/mx.c), XMSG (if/xg.c) - is a `fecall` to
 * the ND-100 front end, and nd500x answers those fecalls itself in
 * nd500_fecall.c. To answer them it has to reach outside the emulator: read a
 * disk image, look up a setting.
 *
 * Until now it reached out through libc directly - a FILE* for the disk. That
 * works on a desktop and is impossible in a browser: a WASM build has no
 * fopen(). It also
 * makes the emulator un-embeddable, because a host that already owns its own
 * storage (nd100x serves disk blocks from OPFS or over its gateway WebSocket)
 * has no way to say so.
 *
 * So the host services become an interface. The DEFAULT implementation is the
 * POSIX one in nd500_host_posix.c and it does exactly what the code did
 * before - getenv() and stdio - so a native build behaves identically and
 * nothing has to be configured. A different host installs its own with
 * nd500_host_set() before the machine starts.
 *
 * SCOPE. This is interface 1 of two (see the plan below): what the ND-500 asks
 * of its *host process*. It is NOT the front-end port - who answers when the
 * ND-500 asks the ND-100 for something - which is a separate seam and is where
 * a real ND-100 will eventually replace the synthetic answers.
 *
 *   E:\Dev\Ronny\NDIX-C\notes\docs\PLAN_TWO_MACHINES_IN_ONE_WASM_2026-08-08.md
 *
 * ONE HOST PER PROCESS. The ops live in a module-level pointer rather than on
 * Nd500Machine, because nd500_fecall.c has helpers with no machine in reach
 * (fedbg(), fe_open_disk()) and threading one through all of them would be a
 * much larger change for no gain today: nd500x runs one ND-500 per process,
 * and even in the eventual ND-100 + ND-500 pairing there is one ND-500 per
 * front end. Revisit only if a second ND-500 ever has to exist side by side.
 */

#ifndef ND500_HOST_H
#define ND500_HOST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Disk units the front end can serve. NDIX's own ceiling is MAXDISK 16
 * (kernel/MASTER/machine/fevar.h), so the guest is already multi-disk and
 * nothing in NDIX has to change to use more than one. Sized to match. */
#define ND500_HOST_MAX_DISKS 16

typedef struct Nd500HostOps {
    /* ---- Block storage --------------------------------------------------
     * Byte offsets, not sectors: the fecall layer already computes an image
     * offset from the guest's device address and sector size, and keeping the
     * ND-500's sector arithmetic out of the host is what lets a host back a
     * unit with something that is not a file at all.
     *
     * unit is 0..ND500_HOST_MAX_DISKS-1.
     * disk_size returns the unit's size in bytes, or -1 if there is no such
     * unit - that is how a host says "this unit is not mounted".
     * disk_read/disk_write return the number of bytes transferred, or -1 on
     * error. A short count is a real short count, not an error. */
    int64_t (*disk_size) (void* ctx, int unit);
    int64_t (*disk_read) (void* ctx, int unit, uint64_t byte_off,
                          void* dst, uint32_t len);
    int64_t (*disk_write)(void* ctx, int unit, uint64_t byte_off,
                          const void* src, uint32_t len);

    /* Non-zero if guest writes to this unit should be honoured. Kept separate
     * from disk_write returning an error so the fecall layer can report the
     * right completion code without attempting the write first. */
    int (*disk_writable)(void* ctx, int unit);
} Nd500HostOps;

/*
 * Install the host services. Pass NULL to fall back to the POSIX default.
 * Call before the machine runs; changing hosts mid-boot is not supported.
 */
void nd500_host_set(const Nd500HostOps* ops, void* ctx);

/*
 * The installed ops and their context. Never returns NULL: if no host has been
 * installed, the POSIX default is installed on first use so existing callers
 * and every test keep working with no setup at all.
 */
const Nd500HostOps* nd500_host(void);
void*               nd500_host_ctx(void);

/* Convenience wrappers - these are what nd500_fecall.c actually calls.
 *
 * NOTE there is no config() here. Settings are NOT a host service: they are
 * plain fields in Nd500Settings (nd500_settings.h), which a host fills in
 * directly. A string-keyed lookup was the first design and it was the wrong
 * one - it kept the key strings, the parsing and the caching inside the
 * emulator for no benefit. */
int64_t     nd500_host_disk_size (int unit);
int64_t     nd500_host_disk_read (int unit, uint64_t byte_off, void* dst, uint32_t len);
int64_t     nd500_host_disk_write(int unit, uint64_t byte_off, const void* src, uint32_t len);
int         nd500_host_disk_writable(int unit);

/*
 * The POSIX default: stdio-backed disks. Exposed so a host can wrap it - for
 * example serve most units itself but keep file-backed ones for testing.
 */
const Nd500HostOps* nd500_host_posix_ops(void);

/*
 * Mount a host FILE-backed image on a unit, for the POSIX default only.
 * `writable` follows ND500X_DISK_RW: 0 = read-only, 1 = write through to the
 * image, 2 = copy-on-write session (a .session copy is made and written to,
 * leaving the master image untouched).
 * Returns 0 on success, -1 on failure.
 */
int  nd500_host_posix_mount(int unit, const char* path, int writable);
void nd500_host_posix_unmount_all(void);

#ifdef __cplusplus
}
#endif

#endif /* ND500_HOST_H */
