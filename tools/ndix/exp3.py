import socket, time, sys
PORT = int(sys.argv[1])
RST = b'\x01\x00\x00\x00\x00\x00\x00\x00'

def conn(pick=b"1\r\n"):
    s = socket.create_connection(("127.0.0.1", PORT), timeout=8)
    time.sleep(0.6)
    try: s.recv(8192)
    except Exception: pass
    s.sendall(pick)
    time.sleep(0.8)
    try: s.recv(8192)
    except Exception: pass
    return s

for i in range(8):
    try:
        s = conn()
        # log in and run a command so there is an ACTIVE SESSION
        s.sendall(b"root\r"); time.sleep(3.0)
        try: s.recv(8192)
        except Exception: pass
        s.sendall(b"who\r"); time.sleep(2.5)
        try: out = s.recv(8192)
        except Exception: out = b""
        got = b"root" in out or b"console" in out
        # abrupt drop mid-session
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, RST)
        s.close()
        time.sleep(0.3)
        # immediate takeover of the same terminal
        s2 = conn()
        time.sleep(0.5)
        s2.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, RST)
        s2.close()
        print("cycle %d done session_output=%s" % (i, got), flush=True)
    except Exception as e:
        print("cycle %d err %s" % (i, e), flush=True)
    time.sleep(1.0)
