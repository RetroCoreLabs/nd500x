# Build a COMPLETE prototype from what is actually in the image.
#
# stage24.proto described an older filesystem: rebuilding from it dropped fsck,
# mount, dump, cron, syslogd and the whole of /lib. This reads a real "ls -l"
# of every root-partition directory, extracts each regular file out of the
# image, and writes a proto that reproduces it exactly - then adds the newly
# built programs on top.
#
# /usr is a separate partition and is deliberately absent: only the root
# partition is rebuilt and spliced, so /usr keeps whatever it already has.
import os, re, subprocess, sys

LISTING = sys.argv[1]
IMG     = sys.argv[2]
STAGE   = sys.argv[3]
OUT     = sys.argv[4]
NEWBIN  = sys.argv[5]
# Optional 6th argument: a directory of plain /etc data files to install.
# A file here whose name matches one already in the image REPLACES it (that is
# how /etc/rc gets edited); a file with a new name is added. Used to put back
# /etc/services, /etc/protocols, /etc/networks and /etc/hosts, which the
# shipped image never had even though NDIX-C's own baseline/etc carries them.
EXTRAETC = sys.argv[6] if len(sys.argv) > 6 else None
extra = {}
if EXTRAETC and os.path.isdir(EXTRAETC):
    for e in sorted(os.listdir(EXTRAETC)):
        p = os.path.join(EXTRAETC, e)
        if os.path.isfile(p):
            extra[e] = p
ND      = "/home/ronny/repos/nd500x/build/bin/nd500x"

def mode_of(perm):
    """'rwxr-xr-x' -> 0755, as a 3-digit octal string."""
    v = 0
    for i, (r, bit) in enumerate([('r',4),('w',2),('x',1)] * 3):
        c = perm[i]
        if c == r or (c in 'sSt' and bit == 1):
            v |= bit << (6 - 3 * (i // 3))
    return "%03o" % v

# ---- parse the listing into {dir: [entries]} ------------------------------
dirs, cur = {}, None
for line in open(LISTING, encoding="utf-8", errors="replace"):
    line = line.rstrip("\n")
    m = re.match(r"^#### (\S+)$", line.strip())
    if m:
        cur = m.group(1); dirs.setdefault(cur, []); continue
    if cur is None:
        continue
    # -rw-r--r--  1 root       77 May  1 17:35 name
    # crw-rw-rw-  1 bin      2,  0 Jul 30  2    name
    m = re.match(r"^([-dbclps])(\S{9})\s+\d+\s+\S+\s+(.*)$", line.strip())
    if not m:
        continue
    typ, perm, rest = m.group(1), m.group(2), m.group(3)
    name = rest.split()[-1]
    if name in (".", "..") or name.startswith("/"):
        continue
    dev = None
    dm = re.match(r"^\s*(\d+),\s*(\d+)\s", rest)
    if dm:
        dev = (int(dm.group(1)), int(dm.group(2)))
    dirs[cur].append((typ, mode_of(perm), name, dev))

# ---- emit ------------------------------------------------------------------
out, recovered, failed = [], 0, 0

def emit_dir(guest, indent):
    global recovered, failed
    for typ, mode, name, dev in sorted(dirs.get(guest, []), key=lambda e: e[2]):
        gpath = (guest.rstrip("/") + "/" + name) if guest != "/" else "/" + name
        pad = "\t" * indent

        if typ == "d":
            if gpath in ("/usr", "/lost+found"):
                # /usr is another partition's mount point; lost+found is made
                # by mkfs. Both are directories that must EXIST and be empty.
                out.append("%s%s d--%s 3 1" % (pad, name, mode))
                out.append("%s$" % pad)
                continue
            out.append("%s%s d--%s 3 1" % (pad, name, mode))
            emit_dir(gpath, indent + 1)
            if gpath == "/etc":
                for b in sorted(os.listdir(NEWBIN)):
                    p = os.path.join(NEWBIN, b)
                    if os.path.isfile(p) and not any(
                            e[2] == b for e in dirs.get("/etc", [])):
                        out.append("%s\t%s ---755 3 1 %s" % (pad, b, p))
                # data files that the image did not have at all. Ones it DID
                # have were already swapped in by the typ == "-" branch below.
                for b, p in sorted(extra.items()):
                    if not any(e[2] == b for e in dirs.get("/etc", [])):
                        out.append("%s\t%s ---644 3 1 %s" % (pad, b, p))
            if gpath == "/dev":
                # Pseudo-terminals. The image had none at all, so telnetd and
                # rlogind failed with "All network ports in use" - which is
                # 4.3's way of saying it could not open a pty master.
                #
                # Majors come from kernel/MASTER/machine/conf.c: entry 5 in
                # cdevsw is pts (the slave, /dev/ttyp?) and entry 6 is ptc (the
                # master, /dev/ptyp?). The GENERIC IDENT sets NPTY=32, so the
                # minors run 0..31 - that is the p series and the q series,
                # 16 each, which is exactly the naming telnetd walks.
                for minor in range(32):
                    series = "pq"[minor // 16]
                    digit  = "0123456789abcdef"[minor % 16]
                    for prefix, major in (("pty", 6), ("tty", 5)):
                        nm = "%s%s%s" % (prefix, series, digit)
                        if not any(e[2] == nm for e in dirs.get("/dev", [])):
                            out.append("%s\t%s c--666 3 1 %d %d"
                                       % (pad, nm, major, minor))
            out.append("%s$" % pad)
            continue

        if typ in "cb" and dev:
            out.append("%s%s %s--%s 3 1 %d %d" % (pad, name, typ, mode, dev[0], dev[1]))
            continue

        if typ == "-":
            # An override wins over whatever is in the image - this is how
            # /etc/rc is edited without ever writing to the live image.
            if guest == "/etc" and name in extra:
                out.append("%s%s ---%s 3 1 %s" % (pad, name, mode, extra[name]))
                recovered += 1
                continue
            local = os.path.join(STAGE, gpath.lstrip("/"))
            os.makedirs(os.path.dirname(local), exist_ok=True)
            r = subprocess.run([ND, "--ndix", IMG, "--extract", gpath, local],
                               capture_output=True, text=True, timeout=180)
            if r.returncode == 0 and os.path.exists(local):
                out.append("%s%s ---%s 3 1 %s" % (pad, name, mode, local))
                recovered += 1
            else:
                print("  COULD NOT READ %s" % gpath, file=sys.stderr)
                failed += 1
            continue

out.append("d--755 3 1")
emit_dir("/", 1)
# extra device nodes for the new terminals
for n in range(3, 9):
    if not any(e[2] == "tty0%d" % n for e in dirs.get("/dev", [])):
        pass   # added below inside /dev by the listing if present
out.append("$")

open(OUT, "w").write("\n".join(out) + "\n")
print("proto: %s  (%d lines, %d files recovered, %d unreadable)"
      % (OUT, len(out), recovered, failed))
