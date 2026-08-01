#!/usr/bin/env python3
"""stage2.py - build the host staging trees for the NDIX disk image.
  $SP/stage/root -> partition a (root fs)   $SP/stage/usr -> partition e (/usr)

Everything already in the shipped image is carried over verbatim from the two
old prototype files (oldfiles.json); the native toolchain and the headers are
added unconditionally; everything else comes from $SP/manifest.txt, whose lines
are

    <path-in-image>   <host-path>   [mode]

Paths under /usr go into the /usr filesystem, everything else into the root
filesystem.  An executable is installed only if its a.out magic is 0x010b
(ZMAGIC, fully linked); anything else is reported and skipped.
"""
import json, os, shutil, struct, subprocess, sys

SP = "/tmp/claude-1000/-home-ronny-repos-ragge-pcc-nd500/430a9137-7e6f-464c-b827-658eaac82a1c/scratchpad"
NB = os.environ["NDIX_B"]
NC = os.environ["NDIX"]
R = SP + "/stage/root"
U = SP + "/stage/usr"
ZMAGIC = 0x010b

rejected = []
installed = []


def is_zmagic(p):
    try:
        with open(p, "rb") as f:
            return struct.unpack(">I", f.read(4))[0] == ZMAGIC
    except Exception:
        return False


def dest(imgpath):
    if imgpath == "/usr" or imgpath.startswith("/usr/"):
        return U + imgpath[4:]
    return R + imgpath


def put(imgpath, src, mode, check):
    d = dest(imgpath)
    if not os.path.exists(src):
        rejected.append((imgpath, src, "source missing"))
        return
    if check and not is_zmagic(src):
        rejected.append((imgpath, src, "not ZMAGIC 0x010b"))
        return
    os.makedirs(os.path.dirname(d), exist_ok=True)
    shutil.copyfile(src, d)
    os.chmod(d, mode)
    installed.append(imgpath)


def main():
    if os.path.exists(SP + "/stage"):
        shutil.rmtree(SP + "/stage")
    old = json.load(open(SP + "/oldfiles.json"))
    for rel, src in old["root"].items():
        exe = rel.startswith("bin/") or rel in (
            "etc/init", "etc/getty", "etc/mount", "etc/umount", "etc/rc")
        put("/" + rel, src, 0o755 if exe else 0o644, exe and rel != "etc/rc")
    for rel, src in old["usr"].items():
        put("/usr/" + rel, src, 0o755, True)

    for d in ("tmp", "usr", "lib"):
        os.makedirs(R + "/" + d, exist_ok=True)
    for d in ("tmp", "lib", "include"):
        os.makedirs(U + "/" + d, exist_ok=True)

    # the native toolchain, at the paths compiled into the cc driver
    for f in ("cc1", "cc2", "cpp", "as", "ld"):
        put("/lib/" + f, SP + "/nat/bin/" + f, 0o755, True)
    put("/lib/crt0.o", SP + "/nat/lib/crt0.o", 0o644, False)
    put("/lib/libc.a", SP + "/nat/lib/libc.a", 0o644, False)
    put("/usr/bin/cc", SP + "/nat/bin/cc", 0o755, True)
    put("/usr/lib/libm.a", SP + "/nat/lib/libm.a", 0o644, False)

    # headers, so the guest can compile
    for f in sorted(os.listdir(NB + "/usr.include")):
        p = NB + "/usr.include/" + f
        if os.path.isfile(p) and f.endswith(".h"):
            put("/usr/include/" + f, p, 0o644, False)
    for sub, srcdir in (("sys", NC + "/kernel/MASTER/h"),
                        ("machine", NC + "/kernel/MASTER/machine"),
                        ("net", NC + "/kernel/MASTER/net"),
                        ("netinet", NC + "/kernel/MASTER/netinet"),
                        ("arpa", NB + "/usr.include/arpa"),
                        ("protocols", SP + "/init/inc/protocols")):
        if not os.path.isdir(srcdir):
            continue
        for f in sorted(os.listdir(srcdir)):
            if f.endswith(".h") and os.path.isfile(srcdir + "/" + f):
                put("/usr/include/%s/%s" % (sub, f), srcdir + "/" + f, 0o644, False)
    for f in ("errno.h", "signal.h", "setjmp.h", "netdb.h", "utmp.h", "ttyent.h"):
        p = SP + "/init/inc/" + f
        if os.path.exists(p):
            put("/usr/include/" + f, p, 0o644, False)

    # everything else
    mf = SP + "/manifest.txt"
    if os.path.exists(mf):
        for line in open(mf):
            line = line.split("#")[0].strip()
            if not line:
                continue
            parts = line.split()
            img, src = parts[0], parts[1]
            # A line with no explicit mode installs a NATIVE BINARY, mode 755,
            # and its a.out magic is checked.  An explicit mode means data or a
            # shell script and is copied as-is.
            if len(parts) > 2:
                put(img, src, int(parts[2], 8), False)
            else:
                put(img, src, 0o755, True)

    print("installed %d files" % len(installed))
    if rejected:
        print("REJECTED / MISSING (%d):" % len(rejected))
        for d_, s, why in rejected:
            print("   %-34s %-70s %s" % (d_, s, why))
    for base, nm in ((R, "root"), (U, "usr")):
        n = sum(len(fs) for _, _, fs in os.walk(base))
        sz = subprocess.run(["du", "-sk", base], capture_output=True,
                            text=True).stdout.split()[0]
        print("%s: %d files, %s KB" % (nm, n, sz))
        for root, dirs, files in os.walk(base):
            cost = 24 + sum(8 + ((len(n2) + 4) // 4) * 4 for n2 in dirs + files)
            if cost > 8192:
                print("DIRECTORY TOO BIG: %s (%d bytes of entries)" % (root, cost))


if __name__ == "__main__":
    main()
