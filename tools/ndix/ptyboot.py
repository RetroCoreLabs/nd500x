#!/usr/bin/env python3
"""Drive ./run-ndix.sh on a REAL pty, type shell commands into the NDIX
console, then send Ctrl-D twice. Usage: ptyboot.py <logfile> [cmd ...]"""
import os, pty, select, sys, time

log = sys.argv[1]
cmds = sys.argv[2:] or ["echo hello", "ls /bin"]

pid, fd = pty.fork()
if pid == 0:
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    os.execv("/bin/bash", ["bash", "./run-ndix.sh"])

out = open(log, "wb")
start = time.time()
sent = 0
next_at = None          # armed once the single-user shell has started
eot = 0
seen = b""
while True:
    now = time.time()
    if now - start > float(os.environ.get('PTYBOOT_HOLD', 150)) + 40:
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
