#!/usr/bin/env python3
"""fswalk.py <image> [blkzero] - walk every directory of an NDIX big-endian FFS
image, following the FULL directory size across all its direct blocks (not just
the first 1024-byte chunk), and report duplicates / zero-length files.
Layout facts from kernel/MASTER/h/{fs.h,inode.h,dir.h} and the mkfs/mkproto
ports: DEV_BSIZE 1024, bsize 8192, dinode 128 bytes, direct = ino(4) reclen(2)
namlen(2) name, each 1024-byte chunk self-contained."""
import sys, struct

IMG = sys.argv[1]
BLKZERO = int(sys.argv[2]) if len(sys.argv) > 2 else 90
f = open(IMG, 'rb')
f.seek(BLKZERO * 1024 + 8192)
sb = f.read(8192)
g = lambda o: struct.unpack('>i', sb[o:o + 4])[0]
iblkno = g(16)
bsize = g(48)
fsize = g(52)
INO_BASE = (BLKZERO + iblkno) * 1024
IFMT, IFCHR, IFBLK, IFDIR, IFREG = 0o170000, 0o020000, 0o060000, 0o040000, 0o100000
bad = 0


def dinode(n):
    f.seek(INO_BASE + n * 128)
    b = f.read(128)
    mode, nlink, uid, gid = struct.unpack('>HHHH', b[0:8])
    size = struct.unpack('>I', b[8:12])[0]
    db = [struct.unpack('>I', b[40 + 4 * k:44 + 4 * k])[0] for k in range(12)]
    return mode, nlink, size, db


def dirdata(size, db):
    """Concatenate the directory's data, following direct blocks."""
    out = b""
    got = 0
    for bno in db:
        if got >= size or bno == 0:
            break
        f.seek((bno + BLKZERO) * 1024)
        n = min(bsize, size - got)
        out += f.read(n)
        got += n
    return out


def walk(size, db):
    d = dirdata(size, db)
    out, off = [], 0
    while off + 8 <= len(d):
        ino, rl, nl = struct.unpack('>IHH', d[off:off + 8])
        if rl == 0:
            break
        nm = d[off + 8:off + 8 + nl].decode('ascii', 'replace')
        if ino:
            out.append((nm, ino))
        off += rl
    return out


def show(path, ino, depth):
    global bad
    mode, nlink, size, db = dinode(ino)
    ents = walk(size, db)
    names = [n for n, i in ents]
    real = [n for n in names if n not in ('.', '..')]
    dups = set(n for n in real if real.count(n) > 1)
    if dups:
        print("DUPLICATE entries in %s: %s" % (path, sorted(dups)))
        bad += 1
    print("%-22s %3d entries (dir size %d): %s" %
          (path, len(real), size, " ".join(sorted(real))))
    for n, i in ents:
        if n in ('.', '..'):
            continue
        m, _, sz, _ = dinode(i)
        if (m & IFMT) == IFDIR and depth < 4:
            show(path.rstrip('/') + '/' + n, i, depth + 1)
        elif (m & IFMT) == IFREG and sz == 0:
            print("  ZERO-LENGTH FILE %s/%s" % (path, n))
            bad += 1


print("iblkno=%d bsize=%d INO_BASE=0x%X" % (iblkno, bsize, INO_BASE))
show('/', 2, 0)
print("BAD: %d" % bad)
