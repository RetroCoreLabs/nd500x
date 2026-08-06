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
#include "ndix_ffs.h"
#include "../../cpu/nd500_fecall.h"
#include "../../cpu/nd500_phys_alloc.h"
#include "../../ndlib/ndlib.h"
#include "../../debugger/commands.h"
#include "../../machine/machine_protos.h"
#include "../../machine/machine_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <sys/stat.h>

#include <ndmon/mon_config.h>
/* setenv() and realpath() are POSIX-only; nd_setenv()/nd_realpath() keep the
 * same behaviour on Windows - including realpath's failure on a missing file,
 * which the "disk image not found" check below depends on. */
#include "nd_compat.h"

/* Set by nd500x_ndix_setup: the kernel path, and whether no <kernel>.init exists
 * so we have to do the boot setup ourselves. */
static char g_auto_kernel[PATH_MAX];

/* The root disk image, kept so the F12 menu can read /etc/utmp out of it and
 * show who is logged in on each line. Set by nd500x_ndix_setup(). */
static char g_image_path[PATH_MAX];
static int  g_auto_boot;


/* ------------------------------------------------------------ boot setup -- */

/* setenv only if the caller has not already chosen a value: --ndix supplies
 * DEFAULTS, and every one of these variables must stay usable as an override. */
static void setenv_default(const char* name, const char* value) {
    const char* cur = getenv(name);
    if (cur && cur[0]) return;
    nd_setenv(name, value, 1);
}

static int is_file(const char* p) {
    struct stat st;
    return p && p[0] && stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

/* The kernel taken out of the disk image, if that is where it came from. It has
 * to exist as a real file for the rest of the boot: `load` reads the a.out from
 * a path and ndlib_symbols_load() takes a path too, so handing over a buffer
 * would mean losing every kernel symbol in the debugger. */
static char g_extracted_kernel[PATH_MAX];

static void remove_extracted_kernel(void) {
    if (g_extracted_kernel[0]) {
        unlink(g_extracted_kernel);
        g_extracted_kernel[0] = '\0';
    }
}

/* Pull <path> out of the filesystem inside <image> and write it to a private
 * temporary file. Returns 0 and fills <out> on success.
 *
 * This is what lets the .img be the only file that has to be delivered: the
 * kernel travels inside the filesystem it boots, exactly like the userland. */
static int extract_kernel(const char* image, const char* path,
                          char* out, size_t outlen) {
    const char* why = "";
    long n = 0;
    uint8_t* data;
    const char* tmpdir;
    char tmpl[PATH_MAX];
    int fd;
    ssize_t written;

    data = ndix_ffs_read_file(image, path, &n, &why);
    if (!data) return -1;
    if (n <= 0) { free(data); return -1; }

    /* Scratch directory for the extracted kernel.
     *
     * TMPDIR is the POSIX name; Windows sets TMP and TEMP instead and has no
     * /tmp at all, so falling straight through to "/tmp" there resolved to
     * <current drive>:\tmp - a directory that usually does not exist, and never
     * the one the user expects. Try all three before giving up. */
    tmpdir = getenv("TMPDIR");
    if (!tmpdir || !tmpdir[0]) tmpdir = getenv("TMP");
    if (!tmpdir || !tmpdir[0]) tmpdir = getenv("TEMP");
    if (!tmpdir || !tmpdir[0]) tmpdir = "/tmp";
    if (snprintf(tmpl, sizeof tmpl, "%s/nd500x-kernel-XXXXXX", tmpdir) >= (int)sizeof tmpl) {
        free(data);
        return -1;
    }
    fd = mkstemp(tmpl);
    if (fd < 0) { free(data); return -1; }

    written = write(fd, data, (size_t)n);
    close(fd);
    free(data);
    if (written != (ssize_t)n) { unlink(tmpl); return -1; }

    snprintf(out, outlen, "%s", tmpl);
    snprintf(g_extracted_kernel, sizeof g_extracted_kernel, "%s", tmpl);
    atexit(remove_extracted_kernel);
    return 0;
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
    if (!nd_realpath(image, abs_image)) {
        fprintf(stderr, "error: --ndix disk image not found: %s\n", image);
        return -1;
    }
    if (!is_file(abs_image)) {
        fprintf(stderr, "error: --ndix disk image is not a regular file: %s\n", abs_image);
        return -1;
    }
    snprintf(g_image_path, sizeof g_image_path, "%s", abs_image);

    /* SINTRAN root: an explicit --sintran-root wins, else the directory the
     * disk image lives in (that is where the NDIX tree is rooted). */
    if (root_opt && root_opt[0]) {
        if (!nd_realpath(root_opt, root)) {
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
        if (!nd_realpath(kernel, kern)) {
            fprintf(stderr, "error: --kernel not found: %s\n", kernel);
            return -1;
        }
    } else {
        const char* env = getenv("ND500X_KERNEL");
        const char* rel[] = { "kernel/MASTER/GENERIC/vmunix", "vmunix" };
        if (env && env[0]) {
            if (!nd_realpath(env, kern)) {
                fprintf(stderr, "error: ND500X_KERNEL not found: %s\n", env);
                return -1;
            }
        } else {
            char cand[PATH_MAX + 64];
            size_t i;
            /* The image's own /vmunix comes FIRST. That is the whole point of
             * shipping one file: the kernel travels inside the filesystem it
             * boots. A kernel rebuilt in the GENERIC directory is therefore NOT
             * picked up on its own - copy it into the image with nd500-mkproto,
             * or point at it with --kernel. The line printed below always says
             * which kernel was actually taken, so this is never a mystery. */
            if (extract_kernel(abs_image, "/vmunix", kern, sizeof kern) == 0) {
                fprintf(stderr, "[ndix] kernel : /vmunix from inside %s\n", abs_image);
            } else {
                for (i = 0; i < sizeof rel / sizeof rel[0]; i++) {
                    snprintf(cand, sizeof cand, "%s/%s", root, rel[i]);
                    if (is_file(cand) && nd_realpath(cand, kern)) break;
                    kern[0] = '\0';
                }
            }
            if (!kern[0]) {
                fprintf(stderr,
                        "error: no NDIX kernel found. Looked for:\n"
                        "         /vmunix inside %s\n"
                        "         %s/kernel/MASTER/GENERIC/vmunix\n"
                        "         %s/vmunix\n"
                        "       Give it explicitly with --kernel <path> or "
                        "ND500X_KERNEL=<path>.\n", abs_image, root, root);
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

/* Put the kernel a.out into physical memory in the layout the PSEG/DSEG files
 * would have produced: text at 0, data at <dseg_load>, bss zeroed after it.
 *
 * The debugger's own `load` is NOT enough here. It places data immediately
 * after text at a_text (0x41A8C for the shipped kernel), while load-dseg places
 * it at the next 2 KB page (0x42000) - and 0x42000 is the address map-kdata is
 * given and the address the kernel's own data references were linked against.
 * The two differ by 1396 bytes, so relying on `load` alone would shift every
 * kernel datum. `load` is still issued before this, for the entry PC and the
 * symbols; this then overwrites what it put down with the right placement. */
static int place_aout_segments(struct Nd500Machine* m, const char* path,
                               unsigned long dseg_load,
                               unsigned long pseg_size, unsigned long dseg_size) {
    unsigned char hdr[32];
    uint32_t a_text, a_data, a_bss;
    unsigned char* buf;
    FILE* f;
    unsigned long i;

    f = fopen(path, "rb");
    if (!f || fread(hdr, 1, sizeof hdr, f) != sizeof hdr) {
        if (f) fclose(f);
        fprintf(stderr, "error: cannot read %s\n", path);
        return -1;
    }
    a_text = ((uint32_t)hdr[4]  << 24) | ((uint32_t)hdr[5]  << 16)
           | ((uint32_t)hdr[6]  << 8)  |  (uint32_t)hdr[7];
    a_data = ((uint32_t)hdr[8]  << 24) | ((uint32_t)hdr[9]  << 16)
           | ((uint32_t)hdr[10] << 8)  |  (uint32_t)hdr[11];
    a_bss  = ((uint32_t)hdr[12] << 24) | ((uint32_t)hdr[13] << 16)
           | ((uint32_t)hdr[14] << 8)  |  (uint32_t)hdr[15];

    /* IMAGIC/OMAGIC keep text immediately after the 32-byte header
     * (pcc-nd500 src/include/nd500/a.out.h, N_TXTOFF). */
    buf = (unsigned char*)malloc((size_t)a_text + a_data);
    if (!buf) { fclose(f); fprintf(stderr, "error: out of memory reading %s\n", path); return -1; }
    if (fread(buf, 1, (size_t)a_text + a_data, f) != (size_t)a_text + a_data) {
        fclose(f); free(buf);
        fprintf(stderr, "error: %s is shorter than its header claims\n", path);
        return -1;
    }
    fclose(f);

    /* Check against the PADDED size, since that is what actually gets written. */
    if (dseg_load + dseg_size > m->memory_size) {
        free(buf);
        fprintf(stderr, "error: kernel needs 0x%lX bytes, machine has 0x%X\n",
                dseg_load + dseg_size, m->memory_size);
        return -1;
    }

    for (i = 0; i < a_text; i++)
        nd500_bus_write8(m, (uint32_t)i, buf[i]);
    /* splitseg pads .pseg with ZEROS from a_text up to the 2 KB page boundary,
     * and the load-pseg path therefore writes those zeros too. We must match it,
     * because the debugger's `load` ran a moment ago and placed the DATA image at
     * a_text (0x41A8C) instead of at the next page (0x42000) - see the comment
     * above. That leaves 1396 bytes of stray data bytes sitting in the text tail
     * which the .pseg path has as zeros.
     *
     * Not cosmetic: those bytes were being read as if they were text, and the
     * boot died dereferencing 0x2025640A - the ASCII of the format string " %d\n"
     * - before reaching login. Zeroing the tail is what makes the from-image boot
     * behave identically to the .pseg/.dseg boot. */
    for (i = a_text; i < pseg_size; i++)
        nd500_bus_write8(m, (uint32_t)i, 0);
    for (i = 0; i < a_data; i++)
        nd500_bus_write8(m, (uint32_t)(dseg_load + i), buf[a_text + i]);
    /* bss must be zero: guest RAM is only cleared at power-on, and the kernel
     * assumes a zeroed bss the way every C program does. The loop runs to
     * dseg_size, not a_data+a_bss, for the same reason as the text tail above:
     * .dseg is padded to a page and map-kdata is handed the padded size. */
    for (i = a_data; i < dseg_size; i++)
        nd500_bus_write8(m, (uint32_t)(dseg_load + i), 0);

    free(buf);

    /* THE separate-I&D data base. This is what load-dseg does after writing the
     * file (commands.c, "Separate I&D de-aliasing for the PSEG/DSEG load path")
     * and it is NOT optional: a.out magic 0411 means the kernel's data lives in
     * D-space at [0, a_data+a_bss), so a segment-0 data read of virtual V must
     * resolve to data_base + V. The debugger's `load`, issued just before us,
     * sets this base to a_text (0x41A8C) because that is where IT put the data.
     * We move the data to the next page (0x42000), so the base has to move with
     * it - otherwise every kernel data read is 1396 bytes low.
     *
     * Measured before this call existed: at _feinit+0x08 the kernel executed
     * `w1 := $114728` and got 0x2025640A (the ASCII of " %d\n" at 0x5DAB4)
     * instead of 0x0003DF00 at 0x5E028 - exactly 1396 bytes adrift - and the
     * boot then died on a page fault at PC=0x3613E. The bytes in memory were
     * correct all along; only this base was wrong. */
    ndlib_aout_set_data_base((uint32_t)dseg_load);

    /* Claim both extents, exactly as the load-pseg / load-dseg commands do via
     * reserve_loaded_image() (src/debugger/commands.c). Without this the page
     * allocator does not know the kernel image is there and can hand the same
     * frames to a domain demand-mapped later. This was the ONLY thing the
     * load-pseg/load-dseg path did that placing the a.out by hand did not, and
     * leaving it out is what made the boot-from-image path die where the
     * .pseg/.dseg path booted. */
    if (nd500_phys_reserve(m, 0, (uint32_t)pseg_size) != 0)
        fprintf(stderr, "warning: kernel text at 0x0 overlaps memory a loaded domain owns\n");
    if (nd500_phys_reserve(m, (uint32_t)dseg_load, (uint32_t)dseg_size) != 0)
        fprintf(stderr, "warning: kernel data at 0x%08lX overlaps memory a loaded domain owns\n",
                dseg_load);

    fprintf(stderr, "[ndix] placed a.out: text 0x0..0x%X (zero-padded to 0x%08lX), "
                    "data 0x%08lX..0x%08lX, bss+pad zeroed to 0x%08lX\n",
            a_text, pseg_size, dseg_load, dseg_load + a_data,
            dseg_load + dseg_size);
    return 0;
}

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
    /* +8 leaves room for the ".pseg"/".dseg" suffix on a PATH_MAX kernel path,
     * which is what gcc's -Wformat-truncation was pointing at. A path that long
     * cannot exist anyway, but sizing the buffer for it is cheaper than an
     * argument about whether snprintf's truncation would matter. */
    char pseg[PATH_MAX + 8], dseg[PATH_MAX + 8], cmd[PATH_MAX + 64];
    struct stat sp, sd;
    unsigned long pseg_size, dseg_load, dseg_size;
    int have_seg_files;

    snprintf(pseg, sizeof pseg, "%s.pseg", g_auto_kernel);
    snprintf(dseg, sizeof dseg, "%s.dseg", g_auto_kernel);
    have_seg_files = (stat(pseg, &sp) == 0 && stat(dseg, &sd) == 0);

    if (have_seg_files) {
        pseg_size = (unsigned long)sp.st_size;
        dseg_size = (unsigned long)sd.st_size;
    } else {
        /* No .pseg/.dseg beside the kernel - derive both from the a.out itself.
         * splitseg produces nothing the header does not already say: pseg is
         * a_text rounded up to a 2 KB page, dseg is a_data + a_bss rounded the
         * same way. Checked against the shipped kernel, whose header reads
         * a_text=0x41A8C a_data=0x1CB20 a_bss=0x21540: that gives 270336 and
         * 256000, byte-for-byte the sizes of vmunix.pseg and vmunix.dseg.
         *
         * This is what lets the kernel come out of the disk image, where only
         * the a.out exists and there are no segment files to sit beside it. */
        uint32_t a_text, a_data, a_bss;
        unsigned char hdr[32];
        FILE* kf = fopen(g_auto_kernel, "rb");
        if (!kf || fread(hdr, 1, sizeof hdr, kf) != sizeof hdr) {
            if (kf) fclose(kf);
            fprintf(stderr, "error: cannot read the a.out header of %s\n", g_auto_kernel);
            return -1;
        }
        fclose(kf);
        /* Big-endian, per pcc-nd500 src/include/nd500/a.out.h: a_magic@0,
         * a_text@4, a_data@8, a_bss@12. */
        a_text = ((uint32_t)hdr[4]  << 24) | ((uint32_t)hdr[5]  << 16)
               | ((uint32_t)hdr[6]  << 8)  |  (uint32_t)hdr[7];
        a_data = ((uint32_t)hdr[8]  << 24) | ((uint32_t)hdr[9]  << 16)
               | ((uint32_t)hdr[10] << 8)  |  (uint32_t)hdr[11];
        a_bss  = ((uint32_t)hdr[12] << 24) | ((uint32_t)hdr[13] << 16)
               | ((uint32_t)hdr[14] << 8)  |  (uint32_t)hdr[15];
        if (a_text == 0) {
            fprintf(stderr, "error: %s has no text segment - not an NDIX kernel\n",
                    g_auto_kernel);
            return -1;
        }
        pseg_size = ((unsigned long)a_text + 0x7FFUL) & ~0x7FFUL;
        dseg_size = (((unsigned long)a_data + a_bss) + 0x7FFUL) & ~0x7FFUL;
        fprintf(stderr, "[ndix] segments derived from the a.out: text=%u data=%u bss=%u\n",
                a_text, a_data, a_bss);
    }

    /* .dseg follows .pseg, page-aligned (NBPG = 2048). */
    dseg_load = (pseg_size + 0x7FFUL) & ~0x7FFUL;

    fprintf(stderr, "[ndix] auto-boot: pseg=%lu dseg=%lu -> dseg@0x%08lX kdata 0x%08lX+0x%lX\n",
            pseg_size, dseg_size, dseg_load, dseg_load, dseg_size);

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
    if (have_seg_files) {
        snprintf(cmd, sizeof cmd, "load-pseg %s 0x00000000", pseg);          run(m, cmd, ctx);
        snprintf(cmd, sizeof cmd, "load-dseg %s 0x%08lX", dseg, dseg_load);  run(m, cmd, ctx);
    } else if (place_aout_segments(m, g_auto_kernel, dseg_load,
                                   pseg_size, dseg_size) != 0) {
        return -1;
    }
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
     * a memory-starved boot does, and died here.
     *
     * _u is looked up in the kernel's own symbol table rather than assumed, so a
     * kernel that moves its u-area still gets a correct vector. The `load` above
     * has already read the symbols. U_CXB0 stays a literal because it is a
     * compile-time struct offset (machine/locore.h:66), not a linker symbol -
     * there is nothing in the a.out to read it from. If the lookup fails we fall
     * back to the measured value rather than booting with THA unset. */
    {
        uint32_t u_addr = 0;
        unsigned long tha = 0xE800073CUL;
        if (ndlib_symbols_lookup("_u", &u_addr, NULL) == 0 && u_addr != 0) {
            tha = (unsigned long)u_addr + 0x73CUL;   /* U_CXB0 = 1852 */
            if (tha != 0xE800073CUL)
                fprintf(stderr, "[ndix] THA derived from _u=0x%08X -> 0x%08lX\n",
                        u_addr, tha);
        } else {
            fprintf(stderr, "[ndix] warning: symbol _u not found, "
                            "using THA 0x%08lX\n", tha);
        }
        snprintf(cmd, sizeof cmd, "set THA 0x%08lX", tha);
        run(m, cmd, ctx);
    }
    run(m, "set CTE1 0xF413D800", ctx);
    run(m, "set CTE2 0x0000005F", ctx);
    run(m, "set CAD 1", ctx);
    return 0;
}

/* ------------------------------------------------------------- shutdown -- */

/* Make the GUEST shut itself down, instead of us just stopping the CPU.
 *
 * Why this is needed: NDIX is a 4.3BSD, so the disk image is only consistent
 * once the kernel has flushed its buffer cache. Killing run_flag leaves every
 * dirty buffer in emulator RAM, and the next boot finds a filesystem that fsck
 * has to repair ("ialloc: dup alloc", "free: freeing free frag" - both were
 * reproduced exactly this way). The image has no /etc/halt, /etc/shutdown or
 * update daemon, so there is nothing inside the guest to ask nicely.
 *
 * What we do instead is what a panic already does: call the kernel's own
 * boot(). machine/machdep.c:985 boot(how, kernel, rdev, cdev) runs update()
 * twice, prints "syncing disks... done", and with RB_BOOT clear takes the HALT
 * branch, ending in feexit_fecall() - which reaches us as FE_EXIT (0xb) and is
 * how we learn the flush finished. We do not have to guess or poll.
 *
 * The work itself is the debugger's `ndix-halt` command, which sits next to
 * `ndix-uarea` - the other NDIX-specific bootstrap command - so that it is also
 * reachable as "~ndix-halt" from the console while the guest is running. That
 * is how this path gets tested without a terminal that can send F12. */
int nd500x_ndix_halt_guest(struct Nd500Machine* m) {
    CmdContext bctx = {0};
    if (!m) return -1;
    return nd500_cmd_execute(m, "ndix-halt", &bctx) == 0 ? 0 : -1;
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

/* The local lines the emulator serves.
 *
 * FE_IDEV reports locdevm = 0xF0000000 (nd500_fecall.c), and io/mx.c:427 reads
 * that as four minors per mask bit counting down from bit 31 - so bits 31-28
 * admit minors 1-16. Eight are offered here, which is well inside that and
 * plenty of terminals for one machine.
 *
 * A line only reaches a login prompt if the IMAGE also has it: a /dev entry
 * (mknod ttyNN c 0 NN) and an /etc/ttys line for getty. Units listed here that
 * the image lacks are simply silent - they cost nothing, and having the table
 * match what the emulator can serve keeps the two ends from drifting apart the
 * way they did when this stopped at tty02.
 *
 * tty81 is deliberately NOT in this list. It is minor 129, the first REMOTE
 * line, and io/mx.c:66 gives every minor from 129 up HARD carrier - open()
 * blocks until the front end reports carrier, the way a dial-in line waits for
 * DCD. That path does not yet complete (the open completion arrives but the
 * line never finishes coming up), so offering it here would advertise a
 * terminal that stays silent. The local lines have soft carrier and just work. */
static NdixTty g_ttys[] = {
    { 0, "console", 0, 0 },
    { 1, "tty01",   0, 0 },
    { 2, "tty02",   0, 0 },
    { 3, "tty03",   0, 0 },
    { 4, "tty04",   0, 0 },
    { 5, "tty05",   0, 0 },
    { 6, "tty06",   0, 0 },
    { 7, "tty07",   0, 0 },
    { 8, "tty08",   0, 0 },
};
#define NDIX_TTY_COUNT ((int)(sizeof g_ttys / sizeof g_ttys[0]))

/* How many of g_ttys[] the running server actually took, so stop() unregisters
 * exactly what start() registered. */
static int g_tty_served = 0;

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

int nd500x_ndix_telnet_start(int port, int count) {
    TelnetServerConfig cfg;
    int i;

    if (g_server) return 0;

    /* count <= 0 means "every terminal the image has". Anything above that is
     * not silently rounded down: a caller asking for six terminals has a wrong
     * idea of the image and should be told, not humoured. */
    if (count <= 0) count = NDIX_TTY_COUNT;
    if (count > NDIX_TTY_COUNT) {
        fprintf(stderr, "[telnet] %d terminals asked for, image has %d "
                        "(console, tty01..tty08) - serving %d\n",
                count, NDIX_TTY_COUNT, NDIX_TTY_COUNT);
        count = NDIX_TTY_COUNT;
    }

    memset(&cfg, 0, sizeof cfg);
    cfg.port = port;
    cfg.maxConnections = count;
    cfg.transport = TRANSPORT_TELNET;

    g_server = TelnetServer_Create(&cfg);
    if (!g_server) {
        fprintf(stderr, "[telnet] cannot create server\n");
        return -1;
    }

    for (i = 0; i < count; i++) {
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
            /* Drop the sinks already installed. A sink outliving its server
             * would send guest output into a freed TelnetServer, and it also
             * suppresses the stdout copy in fe_tty_out - the console would go
             * silent with nothing to show for it. */
            for (--i; i >= 0; i--)
                nd500_fecall_set_tty_output(g_ttys[i].unit, NULL, NULL);
            TelnetServer_Destroy(g_server);
            g_server = NULL;
            return -1;
        }
        nd500_fecall_set_tty_output(g_ttys[i].unit, ndix_tty_out, &g_ttys[i]);
    }

    if (!TelnetServer_Start(g_server)) {
        fprintf(stderr, "[telnet] cannot start server on port %d\n", port);
        for (i = 0; i < count; i++)
            nd500_fecall_set_tty_output(g_ttys[i].unit, NULL, NULL);
        TelnetServer_Destroy(g_server);
        g_server = NULL;
        return -1;
    }
    g_tty_served = count;
    fprintf(stderr, "[telnet] %d guest terminal%s on port %d (%s) - connect with a "
                    "telnet client to localhost %d\n",
            count, count == 1 ? "" : "s", port,
            count == NDIX_TTY_COUNT ? "all" : "first of console, tty01..tty08",
            port);
    return 0;
}

void nd500x_ndix_telnet_stop(void) {
    int i;
    if (!g_server) return;
    for (i = 0; i < g_tty_served; i++)
        nd500_fecall_set_tty_output(g_ttys[i].unit, NULL, NULL);
    g_tty_served = 0;
    TelnetServer_Stop(g_server);
    TelnetServer_Destroy(g_server);
    g_server = NULL;
}

int nd500x_ndix_telnet_active(void) { return g_server != NULL; }

/* ---- status and control for the F12 menu ------------------------------- */

int nd500x_ndix_telnet_port(void) {
    return g_server ? TelnetServer_GetPort(g_server) : 0;
}

int nd500x_ndix_telnet_count(void) {
    return g_server ? TelnetServer_GetTerminalCount(g_server) : 0;
}

int nd500x_ndix_telnet_info(int idx, const char** name, int* connected,
                            char* addr, int addrlen) {
    const char* nm = NULL;
    uint16_t ident = 0;
    bool conn = false, local = false;

    if (addr && addrlen > 0) addr[0] = '\0';
    if (!g_server) return -1;

    /* The server fills clientAddr only while a client is attached, so the
     * caller's buffer is cleared first - otherwise a disconnected line would
     * show whatever the last connected one left behind. */
    if (!TelnetServer_GetTerminalStatus(g_server, idx, &nm, &ident, &conn, &local,
                                        addr, addrlen))
        return -1;

    if (name)      *name = nm;
    if (connected) *connected = conn ? 1 : 0;
    return 0;
}

int nd500x_ndix_telnet_disconnect(int idx) {
    if (!g_server) return -1;
    if (!TelnetServer_DisconnectTerminal(g_server, idx)) return -1;

    /* Log the guest session out too, so the next person to take this line gets
     * a login prompt rather than somebody else's shell.
     *
     * A carrier drop cannot do it. That is the mechanism real hardware uses -
     * io/mx.c:668-687 turns CARRIER_DOWN into gsignal(SIGHUP) - but only on a
     * HARD-carrier line. Every local tty is soft carrier (mxsoftCAR[] is 1 for
     * minors 0-128), and the same code says what happens there:
     *
     *     if (mxsoftCAR[sub]) {
     *         / * since we are ignoring carrier transitions
     *           * we need to repost the read * /
     *
     * - the drop is ignored and the read reposted. Faithfully so: a directly
     * wired terminal has no carrier to lose.
     *
     * So the session is ended from the keyboard side instead, exactly as a
     * person leaving would: interrupt whatever is running, then send
     * end-of-file. INTR is CTRL-C and EOF is CTRL-D (kernel/MASTER/h/
     * ttychars.h: CINTR = CTRL(c), CEOF = CTRL(d)). The shell exits, init
     * respawns getty, and the line comes back at "login:".
     *
     * Limits, stated rather than papered over: a program that ignores SIGINT
     * and does not read stdin will not be dislodged by this, and neither will a
     * line whose owner has turned INTR off with stty. There is no stronger
     * lever available from outside the guest on a soft-carrier line. */
    if (idx >= 0 && idx < NDIX_TTY_COUNT) {
        static const char logout_seq[] = { 0x03, 0x04 };   /* CTRL-C, CTRL-D */
        nd500_fecall_tty_input(g_ttys[idx].unit, logout_seq, (int)sizeof logout_seq);
    }
    return 0;
}

int nd500x_ndix_telnet_pending(void) {
    return g_server ? TelnetServer_GetPendingCount(g_server) : 0;
}

int nd500x_ndix_telnet_mark_local(int unit) {
    int i, found = -1;

    if (!g_server) return -1;

    /* The server already refuses to offer a terminal that is taken - send_menu
     * lists a line only when "clientFd == ND_INVALID_SOCKET && !locallyActive".
     * The second half of that test was dead here, because nothing ever set
     * locallyActive: nd500x never told the server which line the LOCAL window
     * was on. So a telnet client was free to pick the console out of the menu
     * and fight the local terminal for it, two readers on one line.
     *
     * Marking it closes that: exactly one line is locally active at a time -
     * the one this window is attached to - and the telnet menu stops offering
     * it. Every other line is cleared, so switching away with F12 hands the old
     * line back for someone else to use. */
    for (i = 0; i < TelnetServer_GetTerminalCount(g_server); i++) {
        int is_local = (i < NDIX_TTY_COUNT && g_ttys[i].unit == unit);
        TelnetServer_SetTerminalLocallyActive(g_server, i, is_local ? true : false);
        if (is_local) found = i;
    }
    return found >= 0 ? 0 : -1;
}

/* ---- who is logged in, straight out of /etc/utmp ------------------------
 *
 * Deliberately NOT a kernel hook: no symbols, no guest memory, no reading of
 * kernel structures. /etc/utmp is an ordinary file, and the emulator already
 * reads files out of the image with ndix_ffs_read_file() - that is how the
 * kernel itself is extracted at boot.
 *
 * The record layout was MEASURED rather than assumed, by logging in on two
 * lines at once and dumping the file from inside the guest:
 *
 *     36: "console"  44: "root"   68: <time>
 *     72: "tty01"    80: "root"  104: <time>
 *
 * 72 - 36 = 36 bytes per record, with ut_line at +0, ut_name at +8, ut_host at
 * +16 and ut_time at +32 - the classic 4.3BSD struct utmp. The file is indexed
 * by tty slot, so record 0 is unused and the entries line up with /etc/ttys.
 *
 * Two things follow from reading the DISK rather than the running kernel, and
 * both are honest limits rather than bugs:
 *   - a login only shows once utmp has reached the disk through the buffer
 *     cache, so a very fresh one can be missing for a few seconds;
 *   - an image with no /etc/utmp simply reports nobody, which is what the
 *     menu shows anyway when a line is free.
 */
#define UTMP_RECSZ   36
#define UTMP_LINEOFF  0
#define UTMP_NAMEOFF  8
#define UTMP_FIELD    8

int nd500x_ndix_utmp_user(const char* ttyname, char* out, int outlen) {
    uint8_t* data;
    long size = 0;
    const char* why = "";
    long rec;

    if (out && outlen > 0) out[0] = '\0';
    if (!ttyname || !ttyname[0] || !out || outlen <= 0) return -1;
    if (!g_image_path[0]) return -1;

    data = ndix_ffs_read_file(g_image_path, "/etc/utmp", &size, &why);
    if (!data) return -1;

    for (rec = 0; (rec + 1) * UTMP_RECSZ <= size; rec++) {
        const char* line = (const char*)(data + rec * UTMP_RECSZ + UTMP_LINEOFF);
        const char* name = (const char*)(data + rec * UTMP_RECSZ + UTMP_NAMEOFF);
        char lbuf[UTMP_FIELD + 1], nbuf[UTMP_FIELD + 1];

        /* The fields are fixed-width and NOT necessarily terminated, so they
         * are copied out before being compared or returned. */
        memcpy(lbuf, line, UTMP_FIELD); lbuf[UTMP_FIELD] = '\0';
        memcpy(nbuf, name, UTMP_FIELD); nbuf[UTMP_FIELD] = '\0';

        if (lbuf[0] == '\0' || nbuf[0] == '\0') continue;   /* free slot */
        if (strcmp(lbuf, ttyname) != 0) continue;

        snprintf(out, (size_t)outlen, "%s", nbuf);
        free(data);
        return 0;
    }

    free(data);
    return -1;
}

int nd500x_ndix_telnet_in_use(int idx) {
    const char* name = NULL;
    uint16_t ident = 0;
    bool conn = false, local = false;
    char addr[8];

    if (!g_server) return 0;
    if (!TelnetServer_GetTerminalStatus(g_server, idx, &name, &ident, &conn, &local,
                                        addr, (int)sizeof addr))
        return 0;
    return conn ? 1 : 0;
}
