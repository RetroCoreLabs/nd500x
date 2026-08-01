#!/usr/bin/env python3
"""Drive nd500x --ndix on a real pty from an arbitrary cwd."""
import os, pty, select, sys, time
log = sys.argv[1]; argv = sys.argv[2:]
cmds = ["root", "ls -l /etc"]
pid, fd = pty.fork()
if pid == 0:
    os.chdir("/tmp")            # deliberately NOT the kernel dir
    os.execv(argv[0], argv)
out = open(log, "wb"); start = time.time(); seen = b""; sent = 0; next_at = None; eof = 0
while time.time() - start < 200:
    r,_,_ = select.select([fd], [], [], 0.5)
    if r:
        try: data = os.read(fd, 65536)
        except OSError: break
        if not data: break
        out.write(data); out.flush(); seen += data
        if next_at is None and b"/etc/rc running" in seen: next_at = time.time() + 3.0
    if next_at is not None and time.time() >= next_at:
        if sent < len(cmds):
            os.write(fd, (cmds[sent]+"\n").encode()); sent += 1; next_at = time.time() + 12.0
        elif eof < 2:
            os.write(fd, b"\x04"); eof += 1; next_at = time.time() + 6.0
        else: break
out.close(); os.close(fd)
print("done")
