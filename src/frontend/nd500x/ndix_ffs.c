/*
 * ndix_ffs.c - read-only 4.3BSD FFS path lookup for NDIX disk images.
 * See ndix_ffs.h for what this is for and where the layout facts come from.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "ndix_ffs.h"

#include <stdlib.h>
#include <string.h>

/* ---- constants, all read from nd500-mkfs.c ---- */
#define DEV_BSIZE     1024
#define BLKZERO       90            /* raw sector where the FFS partition starts */
#define SBLOCK        8             /* super-block, in DEV_BSIZE sectors */
#define SBSIZE        8192
#define FS_MAGIC      0x011954
#define FS_MAGIC_OFF  1372
#define ROOTINO       2
#define NDADDR        12
#define NIADDR        3
#define SZ_DINODE     128
#define DIRBLKSIZ     1024
#define IFMT          0170000
#define IFDIR         0040000
#define IFREG         0100000

/* dinode field offsets, from serialize_dinode() in nd500-mkproto.c */
#define DI_MODE       0
#define DI_SIZE       8
#define DI_DB         40            /* int32 i_db[NDADDR] */
#define DI_IB         88            /* int32 i_ib[NIADDR] */

/* super-block field offsets. serialize_fs() in nd500-mkfs.c writes these as a
 * run of 32-bit words starting at 8 (fs_link/fs_rlink are skipped), and its own
 * comment checks that the run ends at 208 - which is what pins these down. */
#define SB_IBLKNO     16
#define SB_CGOFFSET   24
#define SB_CGMASK     28
#define SB_BSIZE      48
#define SB_FSIZE      52
#define SB_FRAG       56
#define SB_BMASK      72
#define SB_FRAGSHIFT  96
#define SB_FSBTODB    100
#define SB_NINDIR     116
#define SB_INOPB      120
#define SB_IPG        184
#define SB_FPG        188

/* The largest file this reader will return. A kernel is ~500 KB; anything past
 * a few megabytes means the size field is garbage, and refusing is better than
 * trying to malloc it. */
#define MAX_FILE_BYTES  (64L * 1024L * 1024L)

typedef struct {
    FILE*   f;
    long    base;          /* byte offset of the filesystem inside the image */
    int32_t iblkno, cgoffset, bsize, fsize, frag;
    int32_t fragshift, fsbtodb, nindir, inopb, ipg, fpg;
    uint32_t cgmask, bmask;
} Ffs;

static uint32_t be32(const uint8_t* b) {
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16)
         | ((uint32_t)b[2] << 8)  |  (uint32_t)b[3];
}
static uint16_t be16(const uint8_t* b) {
    return (uint16_t)(((uint32_t)b[0] << 8) | (uint32_t)b[1]);
}

/* Read <len> bytes at filesystem sector <bno> (DEV_BSIZE units, as fsbtodb
 * returns). Returns 0 on success. */
static int rdfs(Ffs* fs, int32_t bno, long len, uint8_t* buf) {
    long off = fs->base + (long)bno * DEV_BSIZE;
    if (bno < 0 || len <= 0) return -1;
    if (fseek(fs->f, off, SEEK_SET) != 0) return -1;
    return fread(buf, 1, (size_t)len, fs->f) == (size_t)len ? 0 : -1;
}

/* The block-addressing helpers, same arithmetic as nd500-mkproto.c:129-137. */
static int32_t fsbtodb(const Ffs* fs, int32_t b) { return b << fs->fsbtodb; }
static int32_t cgbase(const Ffs* fs, int32_t c)  { return fs->fpg * c; }
static int32_t cgstart(const Ffs* fs, int32_t c) {
    return cgbase(fs, c) + fs->cgoffset * (int32_t)(c & ~fs->cgmask);
}
static int32_t cgimin(const Ffs* fs, int32_t c)  { return cgstart(fs, c) + fs->iblkno; }
static int32_t itog(const Ffs* fs, int32_t x)    { return x / fs->ipg; }
static int32_t itoo(const Ffs* fs, int32_t x)    { return x % fs->inopb; }
static int32_t itod(const Ffs* fs, int32_t x) {
    return cgimin(fs, itog(fs, x)) + ((x % fs->ipg) / fs->inopb << fs->fragshift);
}

/* Parse the super-block of an ALREADY OPEN image. The FILE* stays the
 * caller's - nothing here closes it, on success or on failure. This is the
 * half of ffs_open() that does not care where the bytes came from, which is
 * what lets an image held only in memory (fmemopen, in the wasm front end) be
 * read with the same code that reads one on disk. */
static int ffs_attach(Ffs* fs, FILE* f, const char** why) {
    uint8_t sb[SBSIZE];

    memset(fs, 0, sizeof *fs);
    if (!f) { if (why) *why = "cannot open image"; return -1; }
    fs->f = f;
    fs->base = (long)BLKZERO * DEV_BSIZE;

    if (rdfs(fs, SBLOCK, SBSIZE, sb) != 0) {
        if (why) *why = "image too short to hold a super-block";
        fs->f = NULL; return -1;
    }
    if (be32(sb + FS_MAGIC_OFF) != FS_MAGIC) {
        if (why) *why = "no NDIX filesystem (super-block magic mismatch)";
        fs->f = NULL; return -1;
    }

    fs->iblkno    = (int32_t)be32(sb + SB_IBLKNO);
    fs->cgoffset  = (int32_t)be32(sb + SB_CGOFFSET);
    fs->cgmask    =          be32(sb + SB_CGMASK);
    fs->bsize     = (int32_t)be32(sb + SB_BSIZE);
    fs->fsize     = (int32_t)be32(sb + SB_FSIZE);
    fs->frag      = (int32_t)be32(sb + SB_FRAG);
    fs->bmask     =          be32(sb + SB_BMASK);
    fs->fragshift = (int32_t)be32(sb + SB_FRAGSHIFT);
    fs->fsbtodb   = (int32_t)be32(sb + SB_FSBTODB);
    fs->nindir    = (int32_t)be32(sb + SB_NINDIR);
    fs->inopb     = (int32_t)be32(sb + SB_INOPB);
    fs->ipg       = (int32_t)be32(sb + SB_IPG);
    fs->fpg       = (int32_t)be32(sb + SB_FPG);

    /* Magic alone is not enough: a divisor of zero here would be a crash, and
     * these are exactly the fields every later calculation divides or shifts by. */
    if (fs->bsize <= 0 || fs->fsize <= 0 || fs->inopb <= 0 || fs->ipg <= 0
        || fs->nindir <= 0 || fs->fsbtodb < 0 || fs->fragshift < 0
        || fs->bsize % DEV_BSIZE != 0) {
        if (why) *why = "super-block geometry is not usable";
        fs->f = NULL; return -1;
    }
    return 0;
}

static int ffs_open(Ffs* fs, const char* image_path, const char** why) {
    FILE* f = fopen(image_path, "rb");
    if (!f) { if (why) *why = "cannot open image"; return -1; }
    if (ffs_attach(fs, f, why) != 0) { fclose(f); return -1; }
    return 0;
}

static void ffs_close(Ffs* fs) {
    if (fs->f) fclose(fs->f);
    fs->f = NULL;
}

/* Load one dinode. Returns 0 on success. */
static int read_inode(Ffs* fs, int32_t ino, uint8_t out[SZ_DINODE]) {
    uint8_t* blk;
    int32_t off;
    if (ino < ROOTINO) return -1;
    blk = (uint8_t*)malloc((size_t)fs->bsize);
    if (!blk) return -1;
    if (rdfs(fs, fsbtodb(fs, itod(fs, ino)), fs->bsize, blk) != 0) {
        free(blk); return -1;
    }
    off = itoo(fs, ino) * SZ_DINODE;
    if (off < 0 || off + SZ_DINODE > fs->bsize) { free(blk); return -1; }
    memcpy(out, blk + off, SZ_DINODE);
    free(blk);
    return 0;
}

/* Filesystem block number holding byte offset <pos> of this inode, or 0 if the
 * file has no block there (a hole, or past the end). Direct and single-indirect
 * only: NDIX images are built by nd500-mkproto, which itself only ever builds
 * i_ib[0] ("indirect block full" is a fatal error there), so a double indirect
 * cannot occur in an image this reader is meant to read. Anything deeper is
 * reported rather than silently returning wrong bytes. */
/* Read one 32-bit block pointer out of the indirect block <ib>, at <idx>.
 * Returns 0 for a hole (which is also what an unallocated indirect block
 * means), or -1 if the block could not be read at all. */
static int32_t indirect_at(Ffs* fs, int32_t ib, long idx) {
    uint8_t* blk;
    int32_t bno;

    if (ib == 0) return 0;                       /* hole */
    if (idx < 0 || idx >= fs->nindir) return -1; /* out of range for this fs */
    blk = (uint8_t*)malloc((size_t)fs->bsize);
    if (!blk) return -1;
    if (rdfs(fs, fsbtodb(fs, ib), fs->bsize, blk) != 0) { free(blk); return -1; }
    bno = (int32_t)be32(blk + 4 * idx);
    free(blk);
    return bno;
}

/* Map a byte offset in a file to its disk block number.
 *
 * Walks direct, single, double AND triple indirection. Only direct and single
 * used to be handled, and everything else returned an error - which is why
 * /lib/libc.a and /bin/write could not be read out of the image. At an 8 KB
 * block size a file needs double indirection past about 96 KB, so this was not
 * an exotic case: it was every large file. /vmunix (~500 KB) fits in single
 * indirection, which is presumably why the gap went unnoticed - the one big
 * file the emulator read at boot happened to be under the limit. */
static int32_t bmap(Ffs* fs, const uint8_t* din, long pos, const char** why) {
    long lbn = pos / fs->bsize;
    long per = fs->nindir;               /* pointers per indirect block */

    if (lbn < NDADDR)
        return (int32_t)be32(din + DI_DB + 4 * lbn);
    lbn -= NDADDR;

    /* single indirect: i_ib[0] */
    if (lbn < per) {
        int32_t b = indirect_at(fs, (int32_t)be32(din + DI_IB + 0), lbn);
        if (b < 0 && why) *why = "could not read the single indirect block";
        return b;
    }
    lbn -= per;

    /* double indirect: i_ib[1] -> a block of indirect blocks */
    if (lbn < per * per) {
        int32_t mid = indirect_at(fs, (int32_t)be32(din + DI_IB + 4), lbn / per);
        if (mid < 0) { if (why) *why = "could not read the double indirect block"; return -1; }
        if (mid == 0) return 0;
        return indirect_at(fs, mid, lbn % per);
    }
    lbn -= per * per;

    /* triple indirect: i_ib[2]. No NDIX file is anywhere near this big, but
     * handling it costs three lines and removes the last "cannot read" case. */
    if (lbn < per * per * per) {
        int32_t top = indirect_at(fs, (int32_t)be32(din + DI_IB + 8), lbn / (per * per));
        int32_t mid;
        if (top < 0) { if (why) *why = "could not read the triple indirect block"; return -1; }
        if (top == 0) return 0;
        mid = indirect_at(fs, top, (lbn / per) % per);
        if (mid < 0) { if (why) *why = "could not read the triple indirect block"; return -1; }
        if (mid == 0) return 0;
        return indirect_at(fs, mid, lbn % per);
    }

    if (why) *why = "file offset is past even triple indirection";
    return -1;
}

/* Read the whole file described by <din> into a fresh buffer. */
static uint8_t* read_whole(Ffs* fs, const uint8_t* din, long* out_size,
                           const char** why) {
    long size = (long)be32(din + DI_SIZE);
    uint8_t* out;
    long pos;

    if (size < 0 || size > MAX_FILE_BYTES) {
        if (why) *why = "file size is implausible";
        return NULL;
    }
    out = (uint8_t*)calloc(1, (size_t)(size ? size : 1));
    if (!out) { if (why) *why = "out of memory"; return NULL; }

    for (pos = 0; pos < size; pos += fs->bsize) {
        long n = size - pos;
        int32_t bno;
        if (n > fs->bsize) n = fs->bsize;
        bno = bmap(fs, din, pos, why);
        if (bno < 0) { free(out); return NULL; }
        /* Block 0 is a hole: FFS uses it as "not allocated", and calloc has
         * already put the zeroes there. */
        if (bno == 0) continue;
        /* The tail of a file is a fragment, so read only what is left rather
         * than a whole block that may run past the end of the image. */
        if (rdfs(fs, fsbtodb(fs, bno), n, out + pos) != 0) {
            if (why) *why = "short read - image is truncated";
            free(out); return NULL;
        }
    }
    *out_size = size;
    return out;
}

/* Find <name> in the directory described by <din>. Returns its inode number, or
 * 0 if there is no such entry. Directory contents are a run of
 * {ino:4, reclen:2, namlen:2, name[]} records, laid out by entry() in
 * nd500-mkproto.c:446-477, and no record ever straddles a DIRBLKSIZ boundary. */
static int32_t dir_lookup(Ffs* fs, const uint8_t* din, const char* name,
                          const char** why) {
    long dsize = 0;
    uint8_t* d = read_whole(fs, din, &dsize, why);
    long off = 0;
    size_t namelen = strlen(name);
    int32_t found = 0;

    if (!d) return 0;
    while (off + 8 <= dsize) {
        int32_t ino = (int32_t)be32(d + off);
        uint16_t reclen = be16(d + off + 4);
        uint16_t namlen = be16(d + off + 6);

        /* A zero or unaligned reclen would loop forever or walk off the end. */
        if (reclen < 8 || (reclen & 3) != 0) break;

        /* Check the entry BEFORE deciding whether its reclen is sane.
         *
         * The last record in a directory block has its reclen padded out to the
         * end of that block, and that padding can reach past the size recorded
         * in the inode. Treating "off + reclen > dsize" as corruption and
         * breaking first therefore skipped the LAST ENTRY OF EVERY DIRECTORY -
         * which is exactly why /lib/libc.a and /bin/write reported "no such
         * file in the image" while every other name in those directories read
         * out fine. Alphabetically last is not a property a filesystem should
         * care about, and that was the tell.
         *
         * The name itself is still bounds-checked against dsize, so an actually
         * corrupt record cannot make this read past the buffer. */
        if (ino != 0 && namlen == namelen && off + 8 + namlen <= dsize
            && memcmp(d + off + 8, name, namelen) == 0) {
            found = ino;
            break;
        }

        if (off + reclen > dsize) break;   /* that was the final record */
        off += reclen;
    }
    free(d);
    return found;
}

/* The path walk itself, on an image whose super-block is already parsed.
 * Closes nothing: ownership of fs->f belongs to whoever attached it. */
static uint8_t* ffs_read_path(Ffs* fs, const char* path,
                              long* out_size, const char** why) {
    uint8_t din[SZ_DINODE];
    int32_t ino = ROOTINO;
    const char* p = path;
    uint8_t* data = NULL;
    long size = 0;

    if (out_size) *out_size = 0;

    if (read_inode(fs, ino, din) != 0) {
        if (why) *why = "cannot read the root inode";
        return NULL;
    }

    while (*p) {
        char comp[256];
        size_t n = 0;
        while (*p == '/') p++;
        if (!*p) break;
        while (p[n] && p[n] != '/') {
            if (n + 1 >= sizeof comp) {
                if (why) *why = "path component too long";
                return NULL;
            }
            comp[n] = p[n];
            n++;
        }
        comp[n] = '\0';
        p += n;

        /* Only a directory can be walked through. Checking here is what turns
         * "/vmunix/foo" into a clean failure instead of a read of whatever the
         * kernel's first block happens to look like as directory records. */
        if ((be16(din + DI_MODE) & IFMT) != IFDIR) {
            if (why) *why = "path component is not a directory";
            return NULL;
        }
        ino = dir_lookup(fs, din, comp, why);
        if (ino == 0) {
            if (why && (!*why || !(*why)[0])) *why = "no such file in the image";
            return NULL;
        }
        if (read_inode(fs, ino, din) != 0) {
            if (why) *why = "cannot read an inode named by the path";
            return NULL;
        }
    }

    if ((be16(din + DI_MODE) & IFMT) != IFREG) {
        if (why) *why = "not a regular file";
        return NULL;
    }

    data = read_whole(fs, din, &size, why);
    if (data && out_size) *out_size = size;
    return data;
}

uint8_t* ndix_ffs_read_file(const char* image_path, const char* path,
                            long* out_size, const char** why) {
    Ffs fs;
    uint8_t* data;

    if (out_size) *out_size = 0;
    if (why) *why = "";
    if (!image_path || !path) { if (why) *why = "no image or path given"; return NULL; }
    if (ffs_open(&fs, image_path, why) != 0) return NULL;
    data = ffs_read_path(&fs, path, out_size, why);
    ffs_close(&fs);
    return data;
}

uint8_t* ndix_ffs_read_file_fp(FILE* image, const char* path,
                               long* out_size, const char** why) {
    Ffs fs;

    if (out_size) *out_size = 0;
    if (why) *why = "";
    if (!image || !path) { if (why) *why = "no image or path given"; return NULL; }
    if (ffs_attach(&fs, image, why) != 0) return NULL;
    return ffs_read_path(&fs, path, out_size, why);   /* image stays the caller's */
}

int ndix_ffs_probe(const char* image_path, const char** why) {
    Ffs fs;
    if (why) *why = "";
    if (!image_path) { if (why) *why = "no image given"; return 0; }
    if (ffs_open(&fs, image_path, why) != 0) return 0;
    ffs_close(&fs);
    return 1;
}
