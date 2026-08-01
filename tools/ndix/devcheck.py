#!/usr/bin/env python3
# devcheck.py <image> - walk /dev in a big-endian NDIX FFS image and print
# every entry's inode: name, type, mode (octal), major/minor, nlink, size.
# Layout facts verified against kernel source (h/inode.h, h/types.h,
# h/fs.h) and the mkfs/mkproto ports:
#   raw sector 90 = FFS start (BLKZERO); devaddr = frag + 90
#   dinode: 128B; mode@0 u16, nlink@2, uid@4, gid@6, size@8 u32(+4 pad),
#           times@16..39, db[0..11]@40 (db[0]=rdev for specials), ib@88
#   inode i at: itod/itoo -- cg0: inode blocks start at frag 32 (empirically
#   inode 2 dinode at raw 0x1E800+2*128 for this geometry)
#   dir entry: ino u32, reclen u16, namlen u16, name (padded to 4)
import sys, struct

IMG = sys.argv[1] if len(sys.argv) > 1 else 'rootfs_s3.img'
BLKZERO = 90
INO_BASE = 0x1E800          # raw byte offset of dinode 0 (this geometry)
IFMT, IFCHR, IFBLK, IFDIR, IFREG = 0o170000, 0o020000, 0o060000, 0o040000, 0o100000

f = open(IMG, 'rb')

def dinode(n):
    f.seek(INO_BASE + n * 128)
    b = f.read(128)
    mode, nlink, uid, gid = struct.unpack('>HHHH', b[0:8])
    size = struct.unpack('>I', b[8:12])[0]
    db0 = struct.unpack('>I', b[40:44])[0]
    return mode, nlink, uid, gid, size, db0

def readdirblk(frag):
    f.seek((frag + BLKZERO) * 1024)
    return f.read(1024)

def walk(blk):
    out = []
    off = 0
    while off < 1024:
        ino, reclen, namlen = struct.unpack('>IHH', blk[off:off+8])
        if reclen == 0:
            break
        name = blk[off+8:off+8+namlen].decode('ascii', 'replace')
        if ino != 0:
            out.append((name, ino))
        off += reclen
    return out

# root
_, _, _, _, rsize, rdb0 = dinode(2)
root = walk(readdirblk(rdb0))
print('root:', [(n, i) for n, i in root])
dev_ino = dict(root).get('dev')
if not dev_ino:
    print('NO /dev!'); sys.exit(1)
_, _, _, _, dsize, ddb0 = dinode(dev_ino)
print('/dev dir: inode %d size %d frag %d' % (dev_ino, dsize, ddb0))
print('%-10s %-4s %-8s %-6s %-3s %-5s %s' % ('name','ino','mode','type','maj','min','nlink'))
bad = 0
for name, ino in walk(readdirblk(ddb0)):
    mode, nlink, uid, gid, size, db0 = dinode(ino)
    t = mode & IFMT
    ts = {IFCHR:'char', IFBLK:'block', IFDIR:'dir', IFREG:'reg'}.get(t, hex(t))
    maj, mnr = (db0 >> 8) & 0o377, db0 & 0o377
    note = ''
    if name not in ('.','..') and t not in (IFCHR, IFBLK):
        note = ' <-- NOT A SPECIAL NODE'; bad += 1
    if t in (IFCHR, IFBLK) and size != 0:
        note += ' <-- nonzero size'; bad += 1
    if db0 > 0xFFFF:
        note += ' <-- rdev high bits set'; bad += 1
    print('%-10s %-4d 0%-7o %-6s %-3d %-5d %d%s' % (name, ino, mode, ts, maj, mnr, nlink, note))
print('BAD: %d' % bad)
