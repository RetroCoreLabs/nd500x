#!/usr/bin/env python3
"""Drive nd500x DAP: catch the NC codegen crash live, dump state."""
import asyncio, sys, socket, time
sys.path.insert(0, "/home/ronny/repos/libdap/mcp-dap-server")
from mcp_dap_server.dap_connection import DAPConnection

PORT = 4655
NODE = 0x1802A1B0

def wait_port(port, timeout=30):
    t0 = time.time()
    while time.time() - t0 < timeout:
        try:
            s = socket.create_connection(("127.0.0.1", port), timeout=1); s.close(); return True
        except OSError:
            time.sleep(0.5)
    return False

async def main():
    if not wait_port(PORT):
        print("SERVER NEVER CAME UP"); return
    print("port open, connecting")
    conn = DAPConnection()
    await conn.connect("127.0.0.1", PORT, timeout=10)
    r = await conn.send_request("initialize", {"adapterID": "nd500x", "linesStartAt1": True,
        "columnsStartAt1": True, "pathFormat": "path"})
    print("initialize ok:", r.get("success"))
    try:
        await conn.wait_for_event({"initialized"}, timeout=5)
    except Exception as e:
        print("no initialized event:", e)
    # DOM is preloaded via --dom; attach handshake.
    ra = await conn.send_request("attach", {})
    print("attach:", ra.get("success"), ra.get("message",""))
    # Catch all exceptions (the protection violation surfaces as one).
    re = await conn.send_request("setExceptionBreakpoints", {"filters": ["all", "uncaught"]})
    print("setExceptionBreakpoints:", re.get("success"))
    await conn.send_request("configurationDone")
    # Feed the compile command to NC's console.
    rc = await conn.send_request("consoleWrite", {"terminal": 192, "input": "COMPILE A,A,A\r", "hex": False})
    print("consoleWrite:", rc.get("success"), rc.get("message",""))
    # Run; wait for the crash (stopped/exception). May take a while at full speed.
    print("continue -> waiting for crash...")
    await conn.send_request("continue", {"threadId": 1})
    ev = await conn.wait_for_event({"stopped", "exited", "terminated"}, timeout=180)
    print("STOP EVENT:", ev.get("event"), ev.get("body", {}))
    # Dump registers + the doomed node.
    try:
        scopes = await conn.send_request("scopes", {"frameId": 0})
        print("scopes:", [s.get("name") for s in scopes.get("body", {}).get("scopes", [])])
    except Exception as e:
        print("scopes err:", e)
    for name in ("Registers",):
        try:
            st = await conn.send_request("stackTrace", {"threadId": 1})
            frames = st.get("body", {}).get("stackFrames", [])
            print("PC frame:", frames[0] if frames else None)
        except Exception as e:
            print("stackTrace err:", e)
    try:
        mem = await conn.send_request("readMemory", {"memoryReference": hex(NODE), "count": 16})
        print("node[0x%08X] readMemory:" % NODE, mem.get("body", {}))
    except Exception as e:
        print("readMemory err:", e)
    await conn.send_request("disconnect", {"terminateDebuggee": False})
    print("DONE")

asyncio.run(main())
