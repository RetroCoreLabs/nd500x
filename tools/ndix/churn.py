import socket, time, sys
PORT = int(sys.argv[1])
for i in range(20):
    try:
        s = socket.create_connection(("127.0.0.1", PORT), timeout=5)
        time.sleep(0.4)
        try: s.recv(4096)          # menu
        except Exception: pass
        s.sendall(b"1\r\n")        # pick first terminal
        time.sleep(0.5)
        try: s.recv(4096)
        except Exception: pass
        # abrupt close (RST) - worst case for the takeover path
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, b'\x01\x00\x00\x00\x00\x00\x00\x00')
        s.close()
        print("cycle %d ok" % i, flush=True)
    except Exception as e:
        print("cycle %d err %s" % (i, e), flush=True)
    time.sleep(0.6)
