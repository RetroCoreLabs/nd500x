#!/usr/bin/env python3
import socket, sys, time, re
port = int(sys.argv[1]); script = sys.argv[2:]
s = socket.create_connection(("127.0.0.1", port), timeout=20); s.settimeout(2.0)
buf = b""
def pump(sec):
    global buf
    end = time.time()+sec
    while time.time() < end:
        try:
            d = s.recv(65536)
            if not d: return
            buf += d
        except socket.timeout:
            pass
pump(6)
for line in script:
    if line.startswith("SLEEP"): pump(float(line.split()[1])); continue
    s.sendall(line.encode()+b"\r\n"); pump(6)
s.close()
txt = re.sub(rb'\xff[\xfb-\xfe].|\xff.', b'', buf)
sys.stdout.write(txt.decode('latin-1'))
