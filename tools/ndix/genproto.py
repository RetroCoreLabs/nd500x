#!/usr/bin/env python3
"""genproto.py <stagedir> [devspec] - emit an nd500-mkproto prototype file for
a host staging directory tree.  Regular files get mode 755 if the host file is
executable else 644; directories 755, except a directory named "tmp" which gets
777.  If <devspec> is given, its lines "name type maj min mode" are emitted as
special files inside a /dev directory.
Emits absolute host paths, as nd500-mkproto requires.
"""
import os, sys, stat

stage = os.path.abspath(sys.argv[1])
devspec = sys.argv[2] if len(sys.argv) > 2 else None
out = []


def emit(indent, s):
    out.append("\t" * indent + s)


def dirmode(name):
    return "d--777" if name == "tmp" else "d--755"


def walk(path, indent, name=None):
    entries = sorted(os.listdir(path))
    for e in entries:
        p = os.path.join(path, e)
        st = os.stat(p)
        if stat.S_ISDIR(st.st_mode):
            emit(indent, "%s %s 3 1" % (e, dirmode(e)))
            walk(p, indent + 1, e)
            emit(indent + 1, "$")
        else:
            m = "755" if (st.st_mode & 0o100) else "644"
            emit(indent, "%s ---%s 3 1 %s" % (e, m, p))


emit(0, "d--755 3 1")
walk(stage, 1)
if devspec:
    emit(1, "dev d--755 3 1")
    for line in open(devspec):
        line = line.split("#")[0].strip()
        if not line:
            continue
        nm, ty, maj, mnr, mode = line.split()
        emit(2, "%s %s--%s 3 1 %s %s" % (nm, ty, mode, maj, mnr))
    emit(2, "$")
emit(1, "$")
print("\n".join(out))
