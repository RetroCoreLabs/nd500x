#!/usr/bin/env python3
"""ptyrun.py <logfile> <per-cmd-timeout-seconds> <cmd> [cmd ...]

Boot the NDIX guest on a REAL pty, log in as root at the "login:" prompt, then
type each command, waiting for the shell's "# " prompt to come back before
sending the next one (so slow commands like a full compile are not truncated).
Sends Ctrl-D twice at the end.  ND500X_DISK selects the image.
"""
import os, pty, select, sys, time

log = sys.argv[1]
per = float(sys.argv[2])
cmds = sys.argv[3:]

pid, fd = pty.fork()
if pid == 0:
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    os.execv("/bin/bash", ["bash", "./run-ndix.sh", "-N",
                           "-d", os.environ["ND500X_DISK"]])

out = open(log, "wb")
seen = b""
state = "login"          # login -> running -> eot -> done
sent = 0
eot = 0
prompts = 0
deadline = time.time() + 240        # to reach the login prompt
start = time.time()

while True:
    if time.time() > deadline:
        out.write(b"\n*** ptyrun: TIMEOUT in state %s after cmd %d ***\n"
                  % (state.encode(), sent))
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
        prompts = seen.count(b"# ")
    now = time.time()
    if state == "login" and b"login:" in seen:
        time.sleep(1.0)
        os.write(fd, b"root\n")
        state = "running"
        want = 1
        deadline = now + per
    elif state == "running" and prompts >= want:
        if sent < len(cmds):
            time.sleep(0.5)
            os.write(fd, cmds[sent].encode() + b"\n")
            sent += 1
            want = prompts + 1
            deadline = time.time() + per
        else:
            state = "eot"
            deadline = time.time() + 30
    elif state == "eot":
        os.write(fd, b"\x04")
        eot += 1
        time.sleep(3)
        if eot >= 2:
            break

out.write(b"\n*** ptyrun: sent %d/%d commands, %.0fs elapsed ***\n"
          % (sent, len(cmds), time.time() - start))
out.close()
os.close(fd)
print("done: sent %d/%d commands" % (sent, len(cmds)))
