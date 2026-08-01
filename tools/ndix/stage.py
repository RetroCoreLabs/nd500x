#!/usr/bin/env python3
"""stage.py - build the host staging trees for the new NDIX disk image.
  $SP/stage/root -> partition a (root fs)   $SP/stage/usr -> partition e (/usr)
Everything already in the shipped image is carried over verbatim from the two
old prototype files; the new material is added on top.  Only binaries whose
a.out magic is 0x010b (ZMAGIC, fully linked) are installed.
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


def put(dst, src, mode=0o755, check=True):
    if check and not is_zmagic(src):
        rejected.append((dst, src, "not ZMAGIC"))
        return False
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copyfile(src, dst)
    os.chmod(dst, mode)
    installed.append(dst)
    return True


def putdata(dst, src, mode=0o644):
    return put(dst, src, mode, check=False)


def main():
    if os.path.exists(SP + "/stage"):
        shutil.rmtree(SP + "/stage")
    old = json.load(open(SP + "/oldfiles.json"))

    # ---------- carry over everything that is already in the image ----------
    for rel, src in old["root"].items():
        dst = R + "/" + rel
        exe = rel.startswith("bin/") or rel in (
            "etc/init", "etc/getty", "etc/mount", "etc/umount", "etc/rc")
        put(dst, src, 0o755 if exe else 0o644, check=exe and rel != "etc/rc")
    for rel, src in old["usr"].items():
        put(U + "/" + rel, src, 0o755)

    for d in ("tmp", "usr", "lib"):
        os.makedirs(R + "/" + d, exist_ok=True)
    for d in ("tmp", "lib", "include"):
        os.makedirs(U + "/" + d, exist_ok=True)

    # ---------- the native toolchain ----------
    # /lib holds what the compiled-in paths of the cc driver point at
    for f in ("cc1", "cc2", "cpp", "as", "ld"):
        put(R + "/lib/" + f, SP + "/nat/bin/" + f, 0o755)
    putdata(R + "/lib/crt0.o", SP + "/nat/lib/crt0.o")
    putdata(R + "/lib/libc.a", SP + "/nat/lib/libc.a")
    put(U + "/bin/cc", SP + "/nat/bin/cc", 0o755)
    putdata(U + "/lib/libm.a", SP + "/nat/lib/libm.a")

    # ---------- headers, so the guest can actually compile ----------
    inc = U + "/include"
    os.makedirs(inc, exist_ok=True)
    for f in sorted(os.listdir(NB + "/usr.include")):
        p = NB + "/usr.include/" + f
        if os.path.isfile(p) and f.endswith(".h"):
            putdata(inc + "/" + f, p)
    for sub, srcdir in (("sys", NC + "/kernel/MASTER/h"),
                        ("machine", NC + "/kernel/MASTER/machine"),
                        ("net", NC + "/kernel/MASTER/net"),
                        ("netinet", NC + "/kernel/MASTER/netinet"),
                        ("arpa", NB + "/usr.include/arpa"),
                        ("protocols", SP + "/init/inc/protocols")):
        if not os.path.isdir(srcdir):
            continue
        os.makedirs(inc + "/" + sub, exist_ok=True)
        for f in sorted(os.listdir(srcdir)):
            if f.endswith(".h") and os.path.isfile(srcdir + "/" + f):
                putdata(inc + "/" + sub + "/" + f, srcdir + "/" + f)
    # the exact variants the cross build used
    for f in ("errno.h", "signal.h", "setjmp.h", "netdb.h", "utmp.h",
              "ttyent.h"):
        p = SP + "/init/inc/" + f
        if os.path.exists(p):
            putdata(inc + "/" + f, p)

    # ---------- newly built programs ----------
    spec = json.load(open(SP + "/newprogs.json"))
    for dest, names in spec.items():
        base = R if dest.startswith("root:") else U
        sub = dest.split(":", 1)[1]
        for n in names:
            src = SP + "/nat/bin/" + n
            if not os.path.exists(src):
                rejected.append((sub + "/" + n, src, "missing"))
                continue
            put(base + "/" + sub + "/" + n, src, 0o755)

    print("installed %d files" % len(installed))
    if rejected:
        print("REJECTED / MISSING:")
        for d, s, why in rejected:
            print("   %-28s %s (%s)" % (d, s, why))
    for d in (R, U):
        n = sum(len(fs) for _, _, fs in os.walk(d))
        sz = subprocess.run(["du", "-sk", d], capture_output=True,
                            text=True).stdout.split()[0]
        print("%s: %d files, %s KB" % (d, n, sz))
    # directory-size sanity: mkproto packs entries into one 8192-byte fs block
    for root, dirs, files in os.walk(R):
        pass
    for base in (R, U):
        for root, dirs, files in os.walk(base):
            cost = 24 + sum(8 + ((len(n) + 4) // 4) * 4 for n in dirs + files)
            if cost > 8192:
                print("DIRECTORY TOO BIG: %s (%d bytes of entries)" % (root, cost))


if __name__ == "__main__":
    main()
