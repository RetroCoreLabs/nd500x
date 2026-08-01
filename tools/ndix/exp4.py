import socket, time, sys
PORT = int(sys.argv[1]); RST = b'\x01\x00\x00\x00\x00\x00\x00\x00'

def conn(pick):
    s = socket.create_connection(("127.0.0.1", PORT), timeout=8)
    time.sleep(0.6)
    try: s.recv(8192)
    except Exception: pass
    s.sendall(pick + b"\r\n"); time.sleep(0.8)
    try: s.recv(8192)
    except Exception: pass
    return s

def login(s, cmd=b"who\r"):
    s.sendall(b"root\r"); time.sleep(3.0)
    try: s.recv(8192)
    except Exception: pass
    s.sendall(cmd); time.sleep(2.0)
    try: return s.recv(8192)
    except Exception: return b""

for i in range(6):
    try:
        a = conn(b"1")          # console
        b = conn(b"2")          # tty01  - BOTH terminals live at once
        oa = login(a); ob = login(b, b"ls /etc\r")
        # interleave traffic on both while dropping one
        a.sendall(b"who\r"); b.sendall(b"pwd\r"); time.sleep(1.0)
        a.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, RST); a.close()
        time.sleep(0.2)
        c = conn(b"1")          # take console back over immediately
        c.sendall(b"who\r"); time.sleep(1.0)
        b.sendall(b"who\r"); time.sleep(0.8)
        for s in (b, c):
            try:
                s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, RST); s.close()
            except Exception: pass
        print("cycle %d a=%s b=%s" % (i, len(oa) > 0, len(ob) > 0), flush=True)
    except Exception as e:
        print("cycle %d err %s" % (i, e), flush=True)
    time.sleep(1.0)
