#!/usr/bin/env python3
"""Minimal raw-socket telnet client for the nd500x terminal server.
Usage: telcli.py <port> <logfile> <phase-timeout> [-- cmd ...]
Answers IAC negotiation, prints/logs everything received."""
import socket, sys, time, select

port = int(sys.argv[1])
logf = sys.argv[2]
tmo = float(sys.argv[3])
cmds = sys.argv[5:] if len(sys.argv) > 4 else []

IAC, WILL, WONT, DO, DONT, SB, SE = 255, 251, 252, 253, 254, 250, 240

s = socket.create_connection(("127.0.0.1", port), timeout=5)
s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
out = open(logf, "wb")
buf = b""
state = 0
sent = 0
start = time.time()
next_at = start + 2.0
data_seen = bytearray()
while time.time() - start < tmo:
    r, _, _ = select.select([s], [], [], 0.3)
    if r:
        chunk = s.recv(4096)
        if not chunk:
            break
        i = 0
        while i < len(chunk):
            b = chunk[i]
            if state == 0:
                if b == IAC:
                    state = 1
                else:
                    data_seen.append(b)
            elif state == 1:
                if b in (WILL, WONT, DO, DONT):
                    state = b
                elif b == SB:
                    state = 5
                elif b == IAC:
                    data_seen.append(b); state = 0
                else:
                    state = 0
            elif state in (WILL, WONT):
                # answer: DO for ECHO/SGA, DONT otherwise
                s.sendall(bytes([IAC, DO if b in (1, 3) else DONT, b]))
                state = 0
            elif state in (DO, DONT):
                s.sendall(bytes([IAC, WONT, b]))
                state = 0
            elif state == 5:
                if b == IAC:
                    state = 6
            elif state == 6:
                state = 0 if b == SE else 5
            i += 1
        out.write(chunk); out.flush()
    now = time.time()
    if now >= next_at and sent < len(cmds):
        s.sendall((cmds[sent] + "\r\n").encode())
        sent += 1
        next_at = now + 12.0
    if sent >= len(cmds) and cmds and now > next_at:
        break
out.close()
s.close()
sys.stdout.write(data_seen.decode("latin-1"))
