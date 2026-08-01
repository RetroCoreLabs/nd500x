#!/usr/bin/env python3
"""mkar.py [--no-toc] <out.a> <obj...>  - build a 4.3BSD-style archive with a
__.SYMDEF table of contents for the NDIX ND-500 linker.

Format taken from, and only from:
  $NDIX_B/usr.include/ar.h        ARMAG, SARMAG, ar_hdr, ARFMAG
  $NDIX_B/usr.include/ranlib.h    struct ranlib {off_t;off_t}
  $NDIX_B/usr.bin/ranlib.c        which symbols go in the TOC
                                                  (externals, plus commons =
                                                  N_UNDF with n_value != 0);
                                                  the leading word is a BYTE
                                                  count, not an entry count
  $NDIX/baseline/bin/ld/ld.c    getfile(): the TOC member's
                                                  16-byte ar_name must compare
                                                  equal to "__.SYMDEF" over all
                                                  16 bytes -> NUL padded;
                                                  ar_date must be >= the
                                                  archive file's st_mtime
  $NDIX/baseline/bin/ar.c       ordinary members: header
                                                  fields printf'd left
                                                  justified, space padded
  repo src/include/nd500/a.out.h                  32-byte big-endian exec,
                                                  12-byte big-endian nlist

The TOC is written BIG ENDIAN with 4-byte off_t because that is what the
ND-500 target's own ld reads.  A host-built nd500-ld cannot read it (host
off_t is 8 bytes, little endian) - that is expected.

Members are emitted in lorder|tsort order (a user of a symbol comes before
its definer) so that the linker's no-table-of-contents fallback, which makes
a single forward pass, also resolves the library correctly.
"""
import os, struct, sys

ARMAG = b"!<arch>\n"
SARMAG = 8
ARFMAG = b"`\n"
HDRLEN = 60
N_EXT = 0x01
N_UNDF = 0x00
N_TYPE = 0x1e


def arhdr(name, date, uid, gid, mode, size, nulpad=False):
    if nulpad:
        nm = name.encode()[:16].ljust(16, b"\0")
    else:
        nm = ("%-16s" % name).encode()[:16]
    h = nm + ("%-12ld" % date).encode() + ("%-6u" % uid).encode() + \
        ("%-6u" % gid).encode() + ("%-8o" % mode).encode() + \
        ("%-10ld" % size).encode() + ARFMAG
    assert len(h) == HDRLEN, len(h)
    return h


def scan(obj):
    """(defined, referenced) external symbol name sets of an ND-500 a.out .o"""
    d = open(obj, "rb").read()
    (magic, text, data, bss, symsz, entry, trsize, drsize) = \
        struct.unpack(">IIIIIIII", d[:32])
    off = 32 + text + data + trsize + drsize
    strb = d[off + symsz:]
    defd, ref = [], []
    for i in range(symsz // 12):
        strx, typ, other, desc, val = struct.unpack(
            ">IBBHI", d[off + i * 12:off + i * 12 + 12])
        if strx == 0 or not (typ & N_EXT):
            continue
        nm = strb[strx:strb.index(b"\0", strx)].decode("latin1")
        if (typ & N_TYPE) == N_UNDF:
            (ref if val == 0 else defd).append(nm)
        else:
            defd.append(nm)
    return defd, ref


def tsort(members, defs, refs):
    """Order so that every member appears before the members defining the
    symbols it references (lorder | tsort).  Cycles are broken arbitrarily."""
    owner = {}
    for i, m in enumerate(members):
        for s in defs[i]:
            owner.setdefault(s, i)
    # edge i -> j means "i must come before j"
    succ = [set() for _ in members]
    indeg = [0] * len(members)
    for i in range(len(members)):
        for s in refs[i]:
            j = owner.get(s)
            if j is None or j == i or j in succ[i]:
                continue
            succ[i].add(j)
    for i in range(len(members)):
        for j in succ[i]:
            indeg[j] += 1
    order, ready = [], [i for i in range(len(members)) if indeg[i] == 0]
    seen = set()
    while ready:
        i = ready.pop(0)
        if i in seen:
            continue
        seen.add(i)
        order.append(i)
        for j in succ[i]:
            indeg[j] -= 1
            if indeg[j] == 0:
                ready.append(j)
    for i in range(len(members)):      # anything left in a cycle
        if i not in seen:
            order.append(i)
    return order


def main(argv):
    notoc = False
    if argv and argv[0] == "--no-toc":
        notoc = True
        argv = argv[1:]
    out, objs = argv[0], argv[1:]
    names = [os.path.basename(o) for o in objs]
    blobs = [open(o, "rb").read() for o in objs]
    scanned = [scan(o) for o in objs]
    defs = [s[0] for s in scanned]
    refs = [s[1] for s in scanned]
    order = tsort(names, defs, refs)
    names = [names[i] for i in order]
    blobs = [blobs[i] for i in order]
    defs = [defs[i] for i in order]

    strtab, stroff, entries = b"", {}, []
    for idx in range(len(names)):
        for s in defs[idx]:
            if s not in stroff:
                stroff[s] = len(strtab)
                strtab += s.encode() + b"\0"
            entries.append((stroff[s], idx))
    tnum = len(entries) * 8
    tssiz = len(strtab)
    symdefsz = 4 + tnum + 4 + tssiz

    pos = SARMAG
    if not notoc:
        pos += HDRLEN + symdefsz + (symdefsz & 1)
    offs = []
    for b in blobs:
        offs.append(pos)
        pos += HDRLEN + len(b) + (len(b) & 1)

    with open(out, "wb") as f:
        f.write(ARMAG)
        if not notoc:
            toc = struct.pack(">I", tnum)
            for strx, idx in entries:
                toc += struct.pack(">II", strx, offs[idx])
            toc += struct.pack(">I", tssiz) + strtab
            assert len(toc) == symdefsz
            f.write(arhdr("__.SYMDEF", 2147483647, 0, 0, 0o644, symdefsz,
                          nulpad=True))
            f.write(toc)
            if symdefsz & 1:
                f.write(b"\n")
        for nm, b in zip(names, blobs):
            f.write(arhdr(nm, 0, 0, 0, 0o644, len(b)))
            f.write(b)
            if len(b) & 1:
                f.write(b"\n")
    print("%s: %d members, %d TOC entries%s, %d bytes" %
          (out, len(names), len(entries), " (omitted)" if notoc else "",
           os.path.getsize(out)))


if __name__ == "__main__":
    main(sys.argv[1:])
