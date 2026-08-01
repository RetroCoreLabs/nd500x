#!/usr/bin/env python3
"""Drive an nd500x NDIX boot on a REAL pty, type shell commands into the NDIX
console, then send Ctrl-D twice.
Usage: ptyboot2.py <logfile> <timeout> -- <argv...> [-- <cmd> ...]"""
import os, pty, select, sys, time

log = sys.argv[1]
timeout = float(sys.argv[2])
rest = sys.argv[3:]
sep = rest.index("--")
rest = rest[sep + 1:]
if "--" in rest:
    i = rest.index("--")
    argv, cmds = rest[:i], rest[i + 1:]
else:
    argv, cmds = rest, []

pid, fd = pty.fork()
if pid == 0:
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    os.execv(argv[0], argv)

out = open(log, "wb")
start = time.time()
sent = 0
next_at = None
eot = 0
seen = b""
while True:
    if time.time() - start > timeout:
        break
    r, _, _ = select.select([fd], [], [], 0.5)
    if r:
        try:
            data = os.read(fd, 65536)
        except OSError:
            break
        if not data:
            break
        out.write(data)
        out.flush()
        seen += data
        if next_at is None and b"/etc/rc running" in seen:
            next_at = time.time() + 3.0
    now = time.time()
    if next_at is not None and now >= next_at:
        if sent < len(cmds):
            os.write(fd, (cmds[sent] + "\n").encode())
            sent += 1
            next_at = now + 12.0
        elif eot < 2:
            os.write(fd, b"\x04")
            eot += 1
            next_at = now + 8.0
        else:
            break
out.close()
os.close(fd)
try:
    os.waitpid(pid, os.WNOHANG)
except OSError:
    pass
print("done")
