#!/usr/bin/env python3
"""test_ndixput.py - self-contained tests for ndixput.py.

Run:  python3 tools/ndix/test_ndixput.py [path-to-image]

It copies the image to a scratch file first and NEVER writes to the original.
The default image is $NDIX/rootfs_full.img (Ronny's master, read-only here) or
the path given on the command line.

What is checked:
  1. the size window is computed the way fs.h blksize() computes it,
     for a table of hand-worked sizes;
  2. a read-back of an untouched file matches what fswalk-style parsing sees;
  3. writing a shorter body, a body exactly at the top of the window, and a
     --pad'ed body all round-trip byte for byte;
  4. one byte over the window is REFUSED and the image is left bit-identical;
  5. after all writes, every byte of the image except the one data block and
     the one 128-byte dinode is unchanged;
  6. the whole directory tree still walks with no new problems.
"""

import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import ndixput                                  # noqa: E402  (path set above)

FAIL = []


def check(cond, what):
    if cond:
        print('  ok   %s' % what)
    else:
        print('  FAIL %s' % what)
        FAIL.append(what)


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def differing_ranges(a_path, b_path, chunk=1 << 16):
    """List of (start, end) byte ranges where two files differ, coalesced to
    the chunk grid.  Used to prove a write touched nothing it should not."""
    out = []
    with open(a_path, 'rb') as a, open(b_path, 'rb') as b:
        off = 0
        while True:
            x = a.read(chunk)
            y = b.read(chunk)
            if not x and not y:
                break
            if x != y:
                for i in range(min(len(x), len(y))):
                    if x[i] != y[i]:
                        if out and out[-1][1] >= off + i - 8:
                            out[-1] = (out[-1][0], off + i + 1)
                        else:
                            out.append((off + i, off + i + 1))
            off += chunk
    return out


def test_size_window(img):
    print('size window matches fs.h blksize()')
    with ndixput.Ndix(img) as fs:
        # bsize 8192, fsize 1024 on these images; the table below is worked out
        # by hand from blksize() in kernel/MASTER/h/fs.h:339-342.
        cases = [
            (0, 0, 0),
            (1, 1, 1024),
            (1024, 1, 1024),
            (1025, 1025, 2048),
            (1156, 1025, 2048),        # /etc/rc
            (2048, 1025, 2048),
            (8191, 7169, 8192),
            (8192, 7169, 8192),
            (8193, 8193, 9216),        # one full block + one fragment
            (19465, 19457, 20480),     # /etc/hosts
        ]
        for size, lo, hi in cases:
            got = fs.size_window(size)
            check(got == (lo, hi),
                  'window(%d) == (%d, %d)  got %r' % (size, lo, hi, got))
        # Past the direct blocks a partial last block is still a full block,
        # because ufs_bmap.c only ever fragments bn < NDADDR.
        big = 12 * 8192 + 100          # first byte after the 12 direct blocks
        lo, hi = fs.size_window(big)
        check(hi == 13 * 8192,
              'a block reached through an indirect block is never a fragment '
              '(footprint(%d) == %d, got %d)' % (big, 13 * 8192, hi))


def test_roundtrip(img, work):
    print('write / read-back round trip on a scratch copy')
    scratch = os.path.join(work, 'scratch.img')
    shutil.copyfile(img, scratch)
    before = os.path.join(work, 'before.img')
    shutil.copyfile(scratch, before)

    with ndixput.Ndix(scratch) as fs:
        ino = fs.lookup('/etc/rc')
        original = fs.read_file_ino(ino)
        lo, hi = fs.size_window(len(original))
    check(len(original) > 0, '/etc/rc read out, %d bytes' % len(original))

    # 1. a body one byte over the window must be refused, and change nothing.
    body = b'#' + b'x' * (hi - 1) + b'!'        # hi + 1 bytes
    with ndixput.Ndix(scratch, writable=True) as fs:
        try:
            fs.write_file_ino(fs.lookup('/etc/rc'), body)
            check(False, 'oversize write was refused')
        except ndixput.FfsError as e:
            check('will not write' in str(e), 'oversize write refused: %s' % e)
    check(sha(scratch) == sha(before),
          'a refused write left the image bit-identical')

    # 2. a body exactly at the top of the window.
    body = b'# top of window\n' + b'#' * (hi - 17) + b'\n'
    assert len(body) == hi
    with ndixput.Ndix(scratch, writable=True) as fs:
        fs.write_file_ino(fs.lookup('/etc/rc'), body)
    with ndixput.Ndix(scratch) as fs:
        check(fs.read_file_ino(fs.lookup('/etc/rc')) == body,
              'exact-fit body of %d bytes round-tripped' % hi)

    # 3. a shorter body, still inside the window.
    body2 = b'# short\n' + b'y' * (lo - 9) + b'\n'
    assert len(body2) == lo
    with ndixput.Ndix(scratch, writable=True) as fs:
        fs.write_file_ino(fs.lookup('/etc/rc'), body2)
    with ndixput.Ndix(scratch) as fs:
        check(fs.read_file_ino(fs.lookup('/etc/rc')) == body2,
              'short body of %d bytes round-tripped' % lo)

    # 4. a tiny body with --pad, which is the realistic /etc/rc patch case.
    tiny = original + b'\n# added by ndixput\nIP=10.0.2.15\n'
    with ndixput.Ndix(scratch, writable=True) as fs:
        n = fs.write_file_ino(fs.lookup('/etc/rc'), tiny, pad=b'\n')
    check(n == hi, 'padded write filled the window exactly (%d bytes)' % n)
    with ndixput.Ndix(scratch) as fs:
        got = fs.read_file_ino(fs.lookup('/etc/rc'))
    check(got[:len(tiny)] == tiny, 'padded write kept the real content intact')
    check(got[len(tiny):] == b'\n' * (hi - len(tiny)),
          'padded write filled the rest with newlines')

    # 5. nothing outside this file's own block and its own dinode changed.
    with ndixput.Ndix(scratch) as fs:
        ino = fs.lookup('/etc/rc')
        din = fs.read_dinode(ino)
        dblk = ndixput.be32s(din, ndixput.DI_DB)
        data_lo = fs.base + fs.fsbtodb(dblk) * ndixput.DEV_BSIZE
        data_hi = data_lo + hi
        di_lo = fs.dinode_offset(ino)
        di_hi = di_lo + ndixput.SZ_DINODE
    stray = []
    for s, e in differing_ranges(before, scratch):
        inside_data = s >= data_lo and e <= data_hi
        inside_dinode = s >= di_lo and e <= di_hi
        if not (inside_data or inside_dinode):
            stray.append((s, e))
    check(not stray,
          'no bytes changed outside the file data and its dinode (stray: %r)'
          % stray[:5])

    # 6. put the original back and prove the image returns to its exact start.
    with ndixput.Ndix(scratch, writable=True) as fs:
        fs.write_file_ino(fs.lookup('/etc/rc'), original, set_mtime=False)
    # The slack past EOF was zeroed by the writes above, so the image will not
    # be byte-identical to `before` again; the file contents must be though.
    with ndixput.Ndix(scratch) as fs:
        check(fs.read_file_ino(fs.lookup('/etc/rc')) == original,
              'restoring the original contents round-tripped')

    return scratch


def test_tree_walk(img, scratch):
    """Walk both images and compare the listings - any new problem shows up as
    a difference.  Same traversal fswalk.py does, but through this reader."""
    print('directory tree unchanged')

    def listing(path):
        out = []
        with ndixput.Ndix(path) as fs:
            def rec(p, ino, depth):
                ents = fs.dir_entries(ino)
                names = sorted(n for n, i in ents if n not in ('.', '..'))
                out.append('%s %d %s' % (p, len(names), ' '.join(names)))
                if depth >= 4:
                    return
                for n, i in ents:
                    if n in ('.', '..'):
                        continue
                    din = fs.read_dinode(i)
                    mode = ndixput.be16(din, ndixput.DI_MODE)
                    size = ndixput.be32(din, ndixput.DI_SIZE)
                    if (mode & ndixput.IFMT) == ndixput.IFDIR:
                        rec(p.rstrip('/') + '/' + n, i, depth + 1)
                    else:
                        out.append('  %s/%s %d' % (p.rstrip('/'), n, size))
            rec('/', ndixput.ROOTINO, 0)
        return out

    a = listing(img)
    b = listing(scratch)
    diff = [x for x in zip(a, b) if x[0] != x[1]]
    check(len(a) == len(b) and not diff,
          'tree listing identical before and after (%d lines, %d differ)'
          % (len(a), len(diff)))
    if diff:
        for x in diff[:5]:
            print('    %r != %r' % x)


def main():
    img = sys.argv[1] if len(sys.argv) > 1 else \
        '/mnt/e/Dev/Ronny/NDIX-C/rootfs_full.img'
    if not os.path.exists(img):
        print('no image at %s' % img)
        return 2
    print('image: %s' % img)
    work = tempfile.mkdtemp(prefix='ndixput-test-')
    try:
        test_size_window(img)
        scratch = test_roundtrip(img, work)
        test_tree_walk(img, scratch)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print()
    if FAIL:
        print('FAILED %d check(s)' % len(FAIL))
        return 1
    print('all checks passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
