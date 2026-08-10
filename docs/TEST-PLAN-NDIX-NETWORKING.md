# NDIX networking — manual test plan

For a person working through it by hand and writing down what happened.

**What is being tested.** NDIX is 4.3BSD from 1988 running on an emulated Norsk
Data ND-500. It has no network card — it talks to a *media server* over a
message protocol called XMSG, and nd500x provides that server. These tests check
that a 1988 TCP/IP stack can reach the outside world through it.

**Time:** about 45 minutes for the whole plan. Most of it is waiting — the guest
takes roughly 90 seconds to boot and configure itself, and guest commands take
several seconds each. **That is normal, not a fault.**

**Recording results.** Each case has a Result box. Write PASS, FAIL or SKIPPED
and, if it failed, paste what you actually saw. "Didn't work" cannot be acted
on; the exact output usually names the cause.

---

## Before you start

### Prerequisites

| | Check | Expected |
|---|---|---|
| P1 | `docker version` in **PowerShell** | client and server both report a version |
| P2 | `ls /mnt/e/Dev/Ronny/NDIX-C/rootfs_net.img` in WSL | the file exists, about 71 MB |
| P3 | `docker images` in PowerShell | `ronnya/ndix-c` is listed |
| P4 | `ls ~/repos/nd500x/build/bin/nd500x` in WSL | exists (only needed for cases 6–9) |
| P5 | `node --version` in WSL | v18 or later (only needed for cases 10–11) |

**Note on shells.** Docker commands go in **PowerShell**. Emulator and gateway
commands go in a **WSL** shell. Docker Desktop's WSL integration is off on this
machine, so `docker` inside WSL fails with a socket error — that is expected and
is not one of the tests.

### Two things that will otherwise waste your afternoon

**A. Log in as `ndix`, not `root`, over the network.** Root over telnet gives
`Login incorrect` — the same message as a wrong password. That is correct 1988
behaviour, not a bug. `ndix` has no password. Root still works on the console.

**B. `ping` here has no `-c` flag.** It is the 1988 version. `ping -c 3 <host>`
will pretty much pretend to work and print `Network is unreachable` forever. The
usage is `ping <host> [packetsize [count]]`, e.g. `ping 223.255.254.8 56 5`.

---

## Part 1 — a single container

### TC-01 — the container starts and the guest boots

**Steps** (PowerShell):
```powershell
docker run -d --name ndix1 --cap-add=NET_ADMIN --device=/dev/net/tun `
    -e NDIX_ID=1 -p 2301:23 -p 2401:2400 ronnya/ndix-c:dev
```
Wait 90 seconds, then:
```powershell
docker logs ndix1 | Select-String "is UP at"
```

**Expected:** a line containing
`et0 is UP at 223.255.254.8 with NOTRAILERS - the guest is on the network`

**Why NOTRAILERS matters:** if that word is missing, the interface is up but
every IP packet will be silently dropped inside the driver. The interface will
still look perfectly healthy. Treat a missing NOTRAILERS as a FAIL even if
everything else in this case passed.

**Result:** ______

### TC-02 — the container reports itself healthy

**Steps:** `docker ps --format "{{.Names}} {{.Status}}"`

**Expected:** `ndix1 Up ... (healthy)`. The healthcheck connects to the guest's
telnet port, so healthy means the guest is genuinely serving.

If it says `(health: starting)`, wait — the start period is 4 minutes.

**Result:** ______

### TC-03 — the container can reach its guest

**Steps:** `docker exec ndix1 ping -c 4 223.255.254.8`

(This ping is the container's Linux ping, so `-c` is fine here. Only the ping
*inside NDIX* lacks it.)

**Expected:** `4 packets transmitted, 4 received, 0% packet loss`

**Result:** ______

### TC-04 — telnet into the guest and get a shell

**Steps:** `telnet localhost 2301`, then at `login:` type `ndix` and press Enter.
Then run `/bin/hostname` and `/usr/ucb/whoami`.

**Expected:**
```
Release 3 NDIX (nd500x)
login: ndix
NDIX Release 3 on nd500x - Norsk Data ND-5000 emulator.
Welcome back to 1988.
$ /bin/hostname
nd500x
$ /usr/ucb/whoami
ndix
```

**Result:** ______

### TC-05 — the emulator console is reachable

**Steps:** `telnet localhost 2401`, press Enter, log in as `root` (no password),
run `/usr/ucb/netstat -i`.

**Expected:** a table with an `et0` row whose `Ipkts` and `Opkts` are both
non-zero, and `Ierrs`/`Oerrs` both zero.

**Cleanup:** `docker rm -f ndix1`

**Result:** ______

---

## Part 2 — several containers on one wire

### TC-06 — two containers started at the same time

This case exists because starting two at once **used to break**. Start them
together on purpose; do not stagger them.

**Steps** (PowerShell):
```powershell
docker network create ndixnet
docker run -d --name ndix1 --network ndixnet --cap-add=NET_ADMIN --device=/dev/net/tun `
    -e NDIX_ID=1 -e NDIX_PEERS="223.255.254.9@ndix2" -p 2301:23 ronnya/ndix-c:dev
docker run -d --name ndix2 --network ndixnet --cap-add=NET_ADMIN --device=/dev/net/tun `
    -e NDIX_ID=2 -e NDIX_PEERS="223.255.254.8@ndix1" -p 2302:23 ronnya/ndix-c:dev
```
Wait 2 minutes, then `docker ps --format "{{.Names}} {{.Status}}"`

**Expected:** both `(healthy)`.

**A user-defined network is required.** On Docker's default bridge, container
names do not resolve, the peer routes cannot be built, and the guests will never
see each other. If you skip `docker network create` this case fails for that
reason and not because of anything being tested.

**Result:** ______

### TC-07 — no emulator faults in the logs

**Steps** (PowerShell):
```powershell
docker logs ndix1 | Select-String "bad XFGET|No data capability"
docker logs ndix2 | Select-String "bad XFGET|No data capability"
```

**Expected:** **no output at all.** Both of these are known emulator bugs that
were fixed; either one reappearing means a regression. Both produce the same
outward symptom — the interface up and transmitting but receiving nothing —
which is why the log is the reliable way to tell them apart.

**Result:** ______

### TC-08 — the guests reach each other

**Steps:**
```powershell
docker exec ndix1 ping -c 4 223.255.254.9
docker exec ndix2 ping -c 4 223.255.254.8
```

**Expected:** `0% packet loss` both ways.

**Result:** ______

### TC-09 — guest-to-guest from inside NDIX

The strongest case in Part 2: the traffic is generated by the 1988 stack itself.

**Steps:** `telnet localhost 2402` (guest 2's console), log in as `root`, then:
```sh
/etc/ping 223.255.254.8 56 5
```

**Expected:** replies arriving, e.g.
```
64 bytes from 223.255.254.8: icmp_seq=1. time=40. ms
5 packets transmitted, 4 packets received, 20% packet loss
```

**The first packet is often lost.** That is ARP resolving, and it is normal for
this ping. Four of five is a PASS. Zero of five is a FAIL.

**Cleanup:** `docker rm -f ndix1 ndix2` then `docker network rm ndixnet`

**Result:** ______

---

## Part 3 — native, no container

Useful for telling an emulator problem from a Docker problem.

### TC-10 — telnet into a native guest

**Steps** (WSL):
```bash
sudo ~/repos/nd500x/tools/ndix-tap.sh up
cd ~/repos/nd500x
ND500X_ETH_UPLINK=tap ./build/bin/nd500x --ndix /mnt/e/Dev/Ronny/NDIX-C/rootfs_net.img -N
```
At the `login:` prompt type `root`, then:
```sh
/etc/etconfig et0 0x08 0x00 0x26 0xF4 0x01 0x00
/etc/ifconfig et0 inet 223.255.254.8 -trailers up
```
From a second WSL shell: `ping 223.255.254.8` and `telnet 223.255.254.8`.

**Expected:** ping replies, and a login prompt where `ndix` gets a shell.

**Order matters:** `etconfig` must come before `ifconfig`. The interface will not
come up otherwise, and nothing will say why.

**Cleanup:** stop the emulator with Ctrl-D at the guest shell, then
`sudo ~/repos/nd500x/tools/ndix-tap.sh down`.

**Result:** ______

---

## Part 4 — the gateway (browser groundwork)

### TC-11 — gateway unit tests

**Steps** (WSL):
```bash
cd ~/repos/nd100x/tools/nd100-gateway
node test-ethernet.js
node test-eth-wasm.js
```

**Expected:** `13 passed, 0 failed` and `17 passed, 0 failed`.

If `test-eth-wasm.js` says it cannot find the module, build it first:
```bash
cd ~/repos/nd100x && . ~/repos/emsdk/emsdk_env.sh && make wasm
```

**Result:** ______

### TC-12 — real NDIX frames across the gateway

**Steps:** start the gateway with an `ethernet` entry in its config, then start
two native guests with `ND500X_ETH_UPLINK=tcp:127.0.0.1:3094` instead of `tap`.
Configure each as in TC-10 but with different addresses and MACs (`.8`/`F4` and
`.9`/`F5`), then ping one from the other with `/etc/ping 223.255.254.8 56 6`.

**Expected:** `6 packets transmitted, 6 packets received, 0% packet loss`, and
the gateway log showing two `ETH seg=0 TCP client connected` lines.

**Result:** ______

### TC-13 — the browser — EXPLORATORY, not expected to pass

**This hop has never been run.** Everything on both sides of it is tested, but
the WebSocket link between the page and the gateway has not been exercised end to
end. Treat a failure here as information, not a regression, and record exactly
what you saw.

**Steps** (WSL):
```bash
cd ~/repos/nd100x
. ~/repos/emsdk/emsdk_env.sh
make wasm-glass-gateway
```
Open the page, use the **ND-500 window** to pick a kernel and disk and boot NDIX,
then in the **browser console**:
```js
emu.nd500.ethAttach(0)
```
Configure the guest as in TC-10, then ping it from a native guest joined to the
same segment.

**Known gap:** `ethAttach` is not wired to any button. It has to be called from
the console until someone adds a control.

**Result:** ______

---

## Reporting a failure

Include:

1. **Which TC number**, and what you saw versus what was expected.
2. **The container log** if Docker: `docker logs <name> > log.txt`.
3. **`netstat -i` from inside the guest** — `Ipkts`/`Opkts` moving or not is the
   single most useful fact. Transmitting but not receiving is a very different
   problem from neither.
4. **Whether it happened with one machine or only with two.** Both of the
   emulator bugs found so far were invisible with a single guest. If it only
   goes wrong with two, say so prominently.

Background, and the full list of traps: `NDIX-NETWORKING.md`.
Commands without the test scaffolding: `HOWTO-TEST-NDIX-NETWORKING.md`.
