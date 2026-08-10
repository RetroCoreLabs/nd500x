#!/usr/bin/env python3
"""ndixput.py - write a file back INTO an existing NDIX (4.3BSD big-endian FFS)
disk image, in place, and read files out again to check the result.

WHAT THIS DOES AND DELIBERATELY DOES NOT DO
-------------------------------------------
It overwrites the contents of a file that ALREADY EXISTS in the image, and only
when the new contents fit in the disk space the file already owns.  It never
allocates a block, never touches a cylinder-group free map, never creates a
directory entry and never creates an inode.  Everything it changes lives inside
blocks the file already holds, plus four bytes of di_size and the three time
fields of that one inode.  If the new contents do not fit it prints how many
bytes are available versus needed and exits non-zero.  It never truncates
silently.

WHY "FIT" IS NARROWER THAN IT LOOKS
-----------------------------------
FFS gives a file whole 8192-byte blocks except for the very last one, which may
be a run of 1024-byte fragments.  The number of fragments in that last block is
NOT recorded anywhere - it is DERIVED from di_size, by blksize() in
kernel/MASTER/h/fs.h:339-342:

    #define blksize(fs, ip, lbn) \
        (((lbn) >= NDADDR || (ip)->i_size >= ((lbn) + 1) << (fs)->fs_bshift) \
            ? (fs)->fs_bsize \
            : (fragroundup(fs, blkoff(fs, (ip)->i_size))))

fsck uses the same derivation (4.3BSD pass1.c computes id_numfrags from
di_size), so changing di_size across a fragment boundary changes which blocks
the filesystem believes this file owns - without the free maps changing to
match.  That is a real inconsistency even though the data would read back fine.
So the rule enforced here is stricter than "new size <= old size": the new size
must produce the IDENTICAL block/fragment footprint as the old size.  In
practice that is a window of at most 1024 bytes.  Use --pad to land exactly on
the top of that window by appending a filler byte (newline by default), which
is harmless for a shell script such as /etc/rc.

The indirect case is covered too: ufs_bmap.c:77-133 shows only a direct block
(bn < NDADDR) can ever be a fragment; blocks reached through an indirect block
are always a full fs_bsize.  blksize() says the same with its "lbn >= NDADDR".

LAYOUT FACTS AND WHERE THEY COME FROM
-------------------------------------
* Structures: kernel/MASTER/h/fs.h, inode.h, dir.h (read-only, authoritative).
* dinode is 128 bytes; di_mode at 0, di_size at 8 (icommon.ic_size is a `quad`
  and i_size is its val[0], inode.h:43 + inode.h:98), di_atime 16, di_mtime 24,
  di_ctime 32, di_db[12] at 40, di_ib[3] at 88, di_blocks at 104.  Offsets are
  the byte comments in struct icommon, inode.h:40-59.
* Super-block field offsets: same table as
  src/frontend/nd500x/ndix_ffs.c:59-75, which took them from serialize_fs() in
  nd500-mkfs.c.  The super-block sits SBLOCK(=8) DEV_BSIZE sectors into the
  partition, is SBSIZE(=8192) long, magic 0x011954 at byte 1372.
* Inode location arithmetic (cgstart/cgimin/itod/itoo) is fs.h:261-271 and
  ndix_ffs.c:82-91.  fswalk.py takes a shortcut here - it assumes every inode
  lives in cylinder group 0 - which is fine for the small root directories it
  prints but wrong for any inode >= fs_ipg (576 on the root partition).  This
  file does the real arithmetic.
* Directory records: ino(4) reclen(2) namlen(2) name[], no record straddling a
  1024-byte DIRBLKSIZ chunk (dir.h, and entry() in nd500-mkproto.c).
* Partition offsets in the 71147520-byte full image come from
  tools/ndix/mkimage.sh: root FFS partition a starts at block 90 (byte 92160);
  /usr partition e is dd'd in at byte 33546240 and itself starts 90 blocks in,
  so its filesystem base is 33546240 + 92160.
"""

import argparse
import os
import struct
import sys
import time

# ---- constants, all from kernel/MASTER/h/fs.h and inode.h -------------------
DEV_BSIZE = 1024        # fs.h: the raw sector size the fs addresses in
SBLOCK = 8              # fs.h SBLOCK: super-block offset, in DEV_BSIZE sectors
SBSIZE = 8192           # fs.h SBSIZE
FS_MAGIC = 0x011954     # fs.h FS_MAGIC
FS_MAGIC_OFF = 1372     # byte offset of fs_magic inside the super-block
ROOTINO = 2             # fs.h ROOTINO
NDADDR = 12             # inode.h:17
NIADDR = 3              # inode.h:18
SZ_DINODE = 128         # inode.h:65-69, di_size[128]
DIRBLKSIZ = 1024        # dir.h

IFMT = 0o170000
IFDIR = 0o040000
IFREG = 0o100000
IFLNK = 0o120000

# dinode field offsets - the byte comments in struct icommon, inode.h:40-59
DI_MODE = 0
DI_NLINK = 2
DI_UID = 4
DI_GID = 6
DI_SIZE = 8             # ic_size.val[0]; ic_size is a quad, val[1] lives at 12
DI_ATIME = 16
DI_MTIME = 24
DI_CTIME = 32
DI_DB = 40
DI_IB = 88
DI_BLOCKS = 104

# super-block field offsets - identical table to ndix_ffs.c:59-75
SB_IBLKNO = 16
SB_CGOFFSET = 24
SB_CGMASK = 28
SB_BSIZE = 48
SB_FSIZE = 52
SB_FRAG = 56
SB_BSHIFT = 80
SB_FSHIFT = 84
SB_FRAGSHIFT = 96
SB_FSBTODB = 100
SB_NINDIR = 116
SB_INOPB = 120
SB_IPG = 184
SB_FPG = 188

# Partition bases inside the assembled 71147520-byte image (mkimage.sh).
PART_BASES = {
    'root': 90 * DEV_BSIZE,                  # partition a, block 90
    'usr': 33546240 + 90 * DEV_BSIZE,        # partition e, +90 blocks of its own
}

# Refuse to believe a di_size larger than this; a garbage size field should be
# an error, not a multi-gigabyte allocation.  Same idea as ndix_ffs.c:46.
MAX_FILE_BYTES = 64 * 1024 * 1024


class FfsError(Exception):
    """Anything that means "do not touch this image"."""


def be32(b, off=0):
    return struct.unpack_from('>I', b, off)[0]


def be32s(b, off=0):
    return struct.unpack_from('>i', b, off)[0]


def be16(b, off=0):
    return struct.unpack_from('>H', b, off)[0]


class Ndix(object):
    """One FFS filesystem inside an NDIX image.

    Opened read-only by default.  Pass writable=True to get 'r+b'; nothing in
    this class writes unless you call write_file()/set_times().
    """

    def __init__(self, image_path, base=None, writable=False):
        self.path = image_path
        self.writable = writable
        self.f = open(image_path, 'r+b' if writable else 'rb')
        if base is None:
            base = self._autodetect_base()
        self.base = base
        self._read_super()

    # -- open/close ---------------------------------------------------------
    def close(self):
        if self.f:
            self.f.close()
            self.f = None

    def __enter__(self):
        return self

    def __exit__(self, *a):
        self.close()

    def _magic_at(self, base):
        """True if a super-block with the right magic sits at this base."""
        try:
            self.f.seek(base + SBLOCK * DEV_BSIZE + FS_MAGIC_OFF)
            b = self.f.read(4)
        except OSError:
            return False
        return len(b) == 4 and be32(b) == FS_MAGIC

    def _autodetect_base(self):
        """Pick the partition by probing for the super-block magic.

        Both the small 8224768-byte root-only images and the full 71147520-byte
        image put partition a at block 90, so 'root' is tried first and is what
        a caller almost always wants.
        """
        for name in ('root', 'usr'):
            if self._magic_at(PART_BASES[name]):
                return PART_BASES[name]
        raise FfsError('no NDIX filesystem found in %s (no super-block magic '
                       'at byte %d or %d)'
                       % (self.path, PART_BASES['root'], PART_BASES['usr']))

    def _read_super(self):
        self.f.seek(self.base + SBLOCK * DEV_BSIZE)
        sb = self.f.read(SBSIZE)
        if len(sb) != SBSIZE:
            raise FfsError('image too short to hold a super-block')
        if be32(sb, FS_MAGIC_OFF) != FS_MAGIC:
            raise FfsError('no NDIX filesystem at byte %d (super-block magic '
                           'mismatch)' % self.base)
        self.iblkno = be32s(sb, SB_IBLKNO)
        self.cgoffset = be32s(sb, SB_CGOFFSET)
        self.cgmask = be32(sb, SB_CGMASK)          # unsigned; used as ~cgmask
        self.bsize = be32s(sb, SB_BSIZE)
        self.fsize = be32s(sb, SB_FSIZE)
        self.frag = be32s(sb, SB_FRAG)
        self.bshift = be32s(sb, SB_BSHIFT)
        self.fshift = be32s(sb, SB_FSHIFT)
        self.fragshift = be32s(sb, SB_FRAGSHIFT)
        # Named ..._shift and not fs_fsbtodb because fsbtodb() below is a
        # method; the shift count and the function must not share a name.
        self.fsbtodb_shift = be32s(sb, SB_FSBTODB)
        self.nindir = be32s(sb, SB_NINDIR)
        self.inopb = be32s(sb, SB_INOPB)
        self.ipg = be32s(sb, SB_IPG)
        self.fpg = be32s(sb, SB_FPG)
        # Magic alone is not enough - these are exactly the fields every later
        # calculation divides or shifts by (same check as ndix_ffs.c:126-133).
        if (self.bsize <= 0 or self.fsize <= 0 or self.inopb <= 0
                or self.ipg <= 0 or self.nindir <= 0 or self.fsbtodb_shift < 0
                or self.fragshift < 0 or self.bsize % DEV_BSIZE != 0):
            raise FfsError('super-block geometry is not usable')
        if (1 << self.bshift) != self.bsize or (1 << self.fshift) != self.fsize:
            raise FfsError('fs_bshift/fs_fshift disagree with fs_bsize/fs_fsize')

    # -- block addressing, fs.h:261-271 / ndix_ffs.c:82-91 ------------------
    def fsbtodb(self, b):
        return b << self.fsbtodb_shift

    def _byte_of_fsb(self, bno):
        """Byte offset in the image of filesystem block number <bno>."""
        return self.base + self.fsbtodb(bno) * DEV_BSIZE

    def cgbase(self, c):
        return self.fpg * c

    def cgstart(self, c):
        # ~cgmask on a 32-bit unsigned; Python ints are unbounded so mask it.
        return self.cgbase(c) + self.cgoffset * (c & ((~self.cgmask) & 0xFFFFFFFF))

    def cgimin(self, c):
        return self.cgstart(c) + self.iblkno

    def itog(self, ino):
        return ino // self.ipg

    def itoo(self, ino):
        return ino % self.inopb

    def itod(self, ino):
        """Filesystem block number of the inode block holding <ino>."""
        return (self.cgimin(self.itog(ino))
                + ((ino % self.ipg) // self.inopb << self.fragshift))

    def dinode_offset(self, ino):
        """Byte offset in the image of this inode's 128-byte dinode."""
        return self._byte_of_fsb(self.itod(ino)) + self.itoo(ino) * SZ_DINODE

    # -- raw reads ----------------------------------------------------------
    def read_at(self, off, n):
        self.f.seek(off)
        b = self.f.read(n)
        if len(b) != n:
            raise FfsError('short read at byte %d - image is truncated' % off)
        return b

    def read_dinode(self, ino):
        if ino < ROOTINO:
            raise FfsError('inode %d is below ROOTINO' % ino)
        return self.read_at(self.dinode_offset(ino), SZ_DINODE)

    # -- bmap ---------------------------------------------------------------
    def _indirect_at(self, ib, idx):
        """One block pointer out of indirect block <ib> at <idx>.
        0 means a hole (and an unallocated indirect block is all holes)."""
        if ib == 0:
            return 0
        if idx < 0 or idx >= self.nindir:
            raise FfsError('indirect index %d out of range' % idx)
        blk = self.read_at(self._byte_of_fsb(ib), self.bsize)
        return be32s(blk, 4 * idx)

    def bmap(self, din, pos):
        """Disk block number holding byte <pos> of the file, 0 for a hole.
        Direct, single, double and triple indirect - same walk as
        ndix_ffs.c:172-216, which had to be extended past single indirect
        because at bsize 8192 a file needs double indirection past ~96 KB."""
        lbn = pos // self.bsize
        per = self.nindir
        if lbn < NDADDR:
            return be32s(din, DI_DB + 4 * lbn)
        lbn -= NDADDR
        if lbn < per:
            return self._indirect_at(be32s(din, DI_IB + 0), lbn)
        lbn -= per
        if lbn < per * per:
            mid = self._indirect_at(be32s(din, DI_IB + 4), lbn // per)
            if mid == 0:
                return 0
            return self._indirect_at(mid, lbn % per)
        lbn -= per * per
        if lbn < per * per * per:
            top = self._indirect_at(be32s(din, DI_IB + 8), lbn // (per * per))
            if top == 0:
                return 0
            mid = self._indirect_at(top, (lbn // per) % per)
            if mid == 0:
                return 0
            return self._indirect_at(mid, lbn % per)
        raise FfsError('file offset %d is past even triple indirection' % pos)

    # -- whole-file read ----------------------------------------------------
    def read_file_ino(self, ino):
        din = self.read_dinode(ino)
        size = be32(din, DI_SIZE)
        if size > MAX_FILE_BYTES:
            raise FfsError('inode %d claims an implausible size %d' % (ino, size))
        out = bytearray(size)
        pos = 0
        while pos < size:
            n = min(self.bsize, size - pos)
            bno = self.bmap(din, pos)
            if bno != 0:                     # 0 is a hole; bytearray is zeroed
                out[pos:pos + n] = self.read_at(self._byte_of_fsb(bno), n)
            pos += n
        return bytes(out)

    # -- directories --------------------------------------------------------
    def dir_entries(self, ino):
        """[(name, ino)] for a directory inode, in on-disk order."""
        d = self.read_file_ino(ino)
        out = []
        off = 0
        while off + 8 <= len(d):
            eino = be32(d, off)
            reclen = be16(d, off + 4)
            namlen = be16(d, off + 6)
            if reclen < 8 or (reclen & 3) != 0:
                break
            if eino != 0 and off + 8 + namlen <= len(d):
                out.append((d[off + 8:off + 8 + namlen].decode('ascii', 'replace'),
                            eino))
            # The last record of a directory block is padded out to the end of
            # that block and can reach past di_size; check the entry BEFORE
            # deciding reclen is insane, which is the bug fixed in
            # ndix_ffs.c:275-289 (it used to drop the last entry of every dir).
            if off + reclen > len(d):
                break
            off += reclen
        return out

    def lookup(self, path):
        """Resolve an absolute path to an inode number.  No symlink following:
        this tool must know exactly which inode it is about to write."""
        ino = ROOTINO
        for comp in [c for c in path.split('/') if c]:
            din = self.read_dinode(ino)
            if (be16(din, DI_MODE) & IFMT) != IFDIR:
                raise FfsError('%s: path component is not a directory' % path)
            hit = 0
            for name, eino in self.dir_entries(ino):
                if name == comp:
                    hit = eino
                    break
            if hit == 0:
                raise FfsError('%s: no such file in the image' % path)
            ino = hit
        return ino

    # -- the footprint rule -------------------------------------------------
    def blksize(self, size, lbn):
        """blksize() from fs.h:339-342, byte for byte."""
        if lbn >= NDADDR or size >= ((lbn + 1) << self.bshift):
            return self.bsize
        blkoff = size & (self.bsize - 1)        # fs.h:311, loc & ~fs_bmask
        return (blkoff + self.fsize - 1) & ~(self.fsize - 1)   # fragroundup

    def footprint(self, size):
        """Bytes of disk this file owns, derived purely from di_size - exactly
        the way the kernel and fsck derive it.  Sum of blksize() over every
        logical block the file has."""
        nblk = (size + self.bsize - 1) // self.bsize      # howmany(size, bsize)
        total = 0
        for lbn in range(nblk):
            total += self.blksize(size, lbn)
        return total

    def size_window(self, size):
        """(lo, hi) - the inclusive range of new sizes that keep the identical
        footprint as <size>.  hi is the footprint itself; lo is one past the
        largest size with a smaller footprint."""
        fp = self.footprint(size)
        # hi: the biggest size with this footprint is the footprint, except
        # that a full-block file of exactly fp bytes is the top of its own
        # window too - so hi == fp in every case.
        hi = fp
        # lo: search downwards a bounded amount.  The footprint only changes at
        # fragment boundaries, so stepping one fragment back and rounding is
        # enough, but a plain scan of the last fragment is clearer and cheap.
        if fp == 0:
            return (0, 0)
        lo = fp - self.fsize + 1
        if lo < 0:
            lo = 0
        while lo > 0 and self.footprint(lo - 1) == fp:
            lo -= 1
        while lo <= hi and self.footprint(lo) != fp:
            lo += 1
        return (lo, hi)

    # -- the actual write ---------------------------------------------------
    def write_file_ino(self, ino, data, pad=None, set_mtime=True):
        """Overwrite the contents of an EXISTING regular file.

        Refuses unless the new size lands in the same block/fragment footprint
        as the current size.  With <pad> set to a one-byte filler, the data is
        padded up to the top of that window so any content that fits at all is
        accepted.
        """
        if not self.writable:
            raise FfsError('image was opened read-only')
        din = bytearray(self.read_dinode(ino))
        mode = be16(din, DI_MODE)
        if (mode & IFMT) != IFREG:
            raise FfsError('inode %d is not a regular file (mode 0%o)'
                           % (ino, mode))
        oldsize = be32(din, DI_SIZE)
        lo, hi = self.size_window(oldsize)

        if pad is not None and len(data) < hi:
            data = data + pad * (hi - len(data))

        newsize = len(data)
        if newsize < lo or newsize > hi:
            raise FfsError(
                'will not write: %d bytes given, but this file can only hold '
                '%d..%d bytes without changing which blocks it owns '
                '(current size %d, allocated %d bytes in %d-byte blocks / '
                '%d-byte fragments).  Nothing was written.'
                % (newsize, lo, hi, oldsize, hi, self.bsize, self.fsize))

        # Every block the new data needs already exists, because the footprint
        # is unchanged.  Verify that before writing a single byte - a hole here
        # would mean the file has a sparse block and our data would vanish.
        plan = []
        pos = 0
        while pos < newsize:
            n = min(self.bsize, newsize - pos)
            bno = self.bmap(din, pos)
            if bno == 0:
                raise FfsError('file has a hole at byte %d; refusing to write '
                               'because that block is not allocated' % pos)
            plan.append((self._byte_of_fsb(bno), pos, n))
            pos += n

        for off, pos, n in plan:
            self.f.seek(off)
            self.f.write(data[pos:pos + n])

        # Zero the slack between the new end of file and the end of the last
        # allocated fragment.  Not required for correctness - those bytes are
        # past EOF - but it stops stale content from the previous file leaking
        # out if something later grows the file.
        if newsize < hi:
            tail_lbn = newsize // self.bsize
            bno = self.bmap(din, tail_lbn * self.bsize) if newsize else 0
            if bno:
                start_in_blk = newsize - tail_lbn * self.bsize
                blk_alloc = self.blksize(oldsize, tail_lbn)
                if blk_alloc > start_in_blk:
                    self.f.seek(self._byte_of_fsb(bno) + start_in_blk)
                    self.f.write(b'\0' * (blk_alloc - start_in_blk))

        struct.pack_into('>I', din, DI_SIZE, newsize)
        if set_mtime:
            now = int(time.time())
            struct.pack_into('>I', din, DI_MTIME, now)
            struct.pack_into('>I', din, DI_CTIME, now)
        self.f.seek(self.dinode_offset(ino))
        self.f.write(bytes(din))
        self.f.flush()
        os.fsync(self.f.fileno())
        return newsize


# ---- command line ----------------------------------------------------------
def human_stat(fs, path):
    ino = fs.lookup(path)
    din = fs.read_dinode(ino)
    mode = be16(din, DI_MODE)
    size = be32(din, DI_SIZE)
    lo, hi = fs.size_window(size)
    kind = {IFREG: 'file', IFDIR: 'dir', IFLNK: 'symlink'}.get(mode & IFMT,
                                                              'other')
    print('%s  inode %d  %s  mode 0%o  size %d' % (path, ino, kind, mode, size))
    if (mode & IFMT) == IFREG:
        print('  allocated %d bytes; a rewrite may be %d..%d bytes'
              % (hi, lo, hi))


def main(argv=None):
    p = argparse.ArgumentParser(
        description='Write a file into an existing NDIX FFS disk image, '
                    'in place. Only overwrites files that already exist, and '
                    'only within the blocks they already own.')
    p.add_argument('image')
    # path and local are collected as one nargs='*' list rather than two
    # separate positionals: argparse cannot place a trailing optional
    # positional once a flag such as --get appears between them, so
    # "img /etc/rc --get out" lost the last word.  One greedy list has no such
    # problem.
    p.add_argument('rest', nargs='*', metavar='path [local-file]',
                   help='absolute path inside the image (e.g. /etc/rc), then '
                        'optionally the local file to write in')
    p.add_argument('--from-stdin', action='store_true',
                   help='read the new contents from standard input')
    p.add_argument('--get', action='store_true',
                   help='read the file OUT of the image instead of writing; '
                        'to stdout, or to <local> if given')
    p.add_argument('--stat', action='store_true',
                   help='print the inode, size and the writable size window')
    p.add_argument('--list', action='store_true',
                   help='list a directory in the image')
    p.add_argument('--part', choices=sorted(PART_BASES) + ['auto'],
                   default='auto', help='which partition (default: probe)')
    p.add_argument('--base', type=int, default=None,
                   help='byte offset of the filesystem, overrides --part')
    p.add_argument('--pad', metavar='BYTE', default=None,
                   help="pad the new contents up to the top of the allowed "
                        "window with this byte, given as a character or a "
                        "number, e.g. --pad '\\n' or --pad 10")
    p.add_argument('--no-mtime', action='store_true',
                   help='leave di_mtime/di_ctime alone')
    # parse_known_args, not parse_args: argparse stops filling a nargs='*'
    # positional at the first optional flag, so "img /etc/rc --get out.txt"
    # leaves "out.txt" unconsumed.  Anything left over that is not a flag is
    # simply another positional.
    a, leftover = p.parse_known_args(argv)
    bad = [x for x in leftover if x.startswith('-')]
    if bad:
        p.error('unrecognized arguments: %s' % ' '.join(bad))
    a.rest = list(a.rest) + [x for x in leftover if not x.startswith('-')]
    if len(a.rest) < 1 or len(a.rest) > 2:
        p.error('give a path inside the image, and at most one local file')
    a.path = a.rest[0]
    a.local = a.rest[1] if len(a.rest) > 1 else None

    base = a.base
    if base is None and a.part != 'auto':
        base = PART_BASES[a.part]

    read_only = a.get or a.stat or a.list
    try:
        with Ndix(a.image, base=base, writable=not read_only) as fs:
            if a.list:
                ino = fs.lookup(a.path)
                for name, eino in fs.dir_entries(ino):
                    din = fs.read_dinode(eino)
                    print('%7d %7d  %s' % (eino, be32(din, DI_SIZE), name))
                return 0
            if a.stat:
                human_stat(fs, a.path)
                return 0
            if a.get:
                data = fs.read_file_ino(fs.lookup(a.path))
                if a.local:
                    with open(a.local, 'wb') as out:
                        out.write(data)
                else:
                    sys.stdout.buffer.write(data)
                return 0

            if a.from_stdin:
                data = sys.stdin.buffer.read()
            elif a.local:
                with open(a.local, 'rb') as inp:
                    data = inp.read()
            else:
                p.error('give a local file, or --from-stdin, or --get/--stat/--list')

            pad = None
            if a.pad is not None:
                s = a.pad
                if s.isdigit():
                    pad = bytes([int(s)])
                else:
                    pad = s.encode().decode('unicode_escape').encode('latin-1')
                    if len(pad) != 1:
                        p.error('--pad needs exactly one byte')

            ino = fs.lookup(a.path)
            n = fs.write_file_ino(ino, data, pad=pad,
                                  set_mtime=not a.no_mtime)
            print('wrote %d bytes to %s (inode %d) in %s'
                  % (n, a.path, ino, a.image))
            return 0
    except FfsError as e:
        sys.stderr.write('ndixput: %s\n' % e)
        return 1


if __name__ == '__main__':
    sys.exit(main())
