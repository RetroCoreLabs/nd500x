# How to test NDIX networking

Five ways to exercise it, easiest first, with what each one actually proves and
what it does not. Every command here has been run; where something is untested
it says so.

Background and the traps: `NDIX-NETWORKING.md` in this directory.

---

## The two things every method needs

**1. `-trailers`, always.** NDIX's ARP advertises trailer encapsulation its own
`et` driver refuses to send, so without the flag **every IP packet is dropped
inside the driver** while the interface looks perfectly healthy — UP, RUNNING,
ARP table filling, counters moving. Look for `NOTRAILERS` in the flags:

```
et0: flags=63<UP,BROADCAST,NOTRAILERS,RUNNING>
```

**2. Log in as `ndix`, not `root`, over the network.** `/bin/login` refuses root
on a non-secure tty and prints only `Login incorrect` — the same message as a
wrong password. `rootfs_net.img` carries an ordinary `ndix` account with no
password. Root still works on the console.

---

## 1. Telnet into NDIX in a container

The quickest complete test. The image on disk already contains the emulator
fixes.

**Run these from PowerShell.** Docker Desktop's WSL integration is off on this
machine, so `docker` from a WSL shell reports `/var/run/docker.sock` missing.
(Fix it if you like: Docker Desktop → Settings → General → "Use the WSL 2 based
engine". It is a convenience, not a requirement — `/dev/net/tun` and
`CAP_NET_ADMIN` both work on the Hyper-V backend.)

```powershell
docker run -d --name ndix1 --cap-add=NET_ADMIN --device=/dev/net/tun `
    -e NDIX_ID=1 -p 2301:23 -p 2401:2400 ronnya/ndix-c:dev

# ~90 seconds to boot and configure itself
docker logs ndix1 | Select-String "is UP at"

telnet localhost 2301     # the GUEST. log in as: ndix
telnet localhost 2401     # the EMULATOR CONSOLE. root works here
docker rm -f ndix1
```

**Proves:** the emulator, XMSG, the driver, IP, inetd, telnetd, pty allocation
and login — the whole stack, from outside the container.

To rebuild the image after changing nd500x:

```powershell
cd \\wsl.localhost\Ubuntu\home\ronny\repos\nd500x
docker build -f docker/Dockerfile -t ronnya/ndix-c:dev `
    --build-arg NDIX_IMAGE_SRC=docker/disk/rootfs_net.img .
```

## 2. Several containers on one wire

```powershell
docker network create ndixnet
docker run -d --name ndix1 --network ndixnet --cap-add=NET_ADMIN --device=/dev/net/tun `
    -e NDIX_ID=1 -e NDIX_PEERS="223.255.254.9@ndix2" -p 2301:23 ronnya/ndix-c:dev
docker run -d --name ndix2 --network ndixnet --cap-add=NET_ADMIN --device=/dev/net/tun `
    -e NDIX_ID=2 -e NDIX_PEERS="223.255.254.8@ndix1" -p 2302:23 ronnya/ndix-c:dev

# ~2 minutes, then:
docker exec ndix1 ping -c 4 223.255.254.9
docker exec ndix2 ping -c 4 223.255.254.8
docker ps --format "{{.Names}} {{.Status}}"      # both should say (healthy)

docker rm -f ndix1 ndix2 && docker network rm ndixnet
```

**A user-defined network is required** — on the default bridge, container names
do not resolve, so `NDIX_PEERS` cannot build its routes and the guests never see
each other. Both containers may be started **at the same time**; that used to
break and no longer does.

**Proves:** two guests on separate routed segments reaching each other, and that
neither of the two emulator bugs below reappears.

Expected: `4 packets transmitted, 4 received, 0% packet loss`.

## 3. Native, from a WSL shell

```bash
sudo ~/repos/nd500x/tools/ndix-tap.sh up          # once, needs root
cd ~/repos/nd500x
ND500X_ETH_UPLINK=tap ./build/bin/nd500x --ndix /mnt/e/Dev/Ronny/NDIX-C/rootfs_net.img -N
```

At the `login:` prompt use `root` (no password), then:

```sh
/etc/etconfig et0 0x08 0x00 0x26 0xF4 0x01 0x00
/etc/ifconfig et0 inet 223.255.254.8 -trailers up
```

From another shell: `ping 223.255.254.8`, `telnet 223.255.254.8` (as `ndix`).
`~/repos/nd500x/tools/ndix-tap.sh status` says whether anything has the device
open and whether the guest answers.

**Proves:** the same stack with no container in the way — useful when you want
to know whether a problem is Docker's or the emulator's.

## 4. The gateway — the Phase D piece

Unit tests, no emulator and no browser:

```bash
cd ~/repos/nd100x/tools/nd100-gateway
node test-ethernet.js      # 13 checks: the segment, three members, framing
node test-eth-wasm.js      # 17 checks: the real wasm module loaded under node
```

`test-ethernet.js` uses **three** members on purpose. Ethernet is multipoint and
HDLC is not; a version that kept one socket per segment would pass with two
members and silently drop the third.

With real NDIX traffic — start the gateway with an `ethernet` entry, then join
two native guests to it:

```bash
ND500X_ETH_UPLINK=tcp:127.0.0.1:3094 ./build/bin/nd500x --ndix <img> -N
```

Configure both as in method 3 (different addresses and MACs) and ping. Measured:
`6 packets transmitted, 6 packets received, 0% packet loss`.

**Proves:** the gateway's RETH server, its handshake, its framing and its
repeat-to-all, carrying frames a real 1988 driver produced.

## 5. The browser — NOT PROVEN

```bash
cd ~/repos/nd100x
. ~/repos/emsdk/emsdk_env.sh        # emsdk lives INSIDE repos/, and must be sourced
make wasm-glass-gateway             # builds the glass UI and starts the gateway
```

Open the page, use the **ND-500 window** to pick a kernel and a disk and boot
NDIX, then from the **browser console**:

```js
emu.nd500.ethAttach(0)
```

Then configure the guest exactly as in method 3, and ping it from a native guest
joined to the same segment.

**Two honest caveats.**

`ethAttach` is callable but **is not wired to a button** — nothing in
`nd500-window.js` calls it, so it has to come from the console until someone adds
a control.

**This hop has never been run.** Everything on both sides of it is tested — the
gateway carries real NDIX frames (method 4) and the module's exports behave under
node — but the WebSocket link between them has not been exercised end to end. If
it fails, that is where to look first.

---

## What to check when something does not work

| Symptom | Cause |
|---|---|
| Interface UP, ARP fine, every IP packet fails with `Protocol not supported` | `-trailers` missing on one or both ends |
| `Login incorrect` for root over telnet | expected; use the `ndix` account |
| `ping -c 3 host` prints `Network is unreachable` forever | this ping has no `-c`. Usage is `ping host [size [count]]` |
| Guests cannot see each other in Docker | default bridge — container names do not resolve. Use a user-defined network |
| `docker` fails in WSL with a socket error | WSL integration is off; use PowerShell |
| `emcc: command not found` | `. ~/repos/emsdk/emsdk_env.sh` first; it is at `~/repos/emsdk`, not `~/emsdk` |
| `et0` up, transmitting, receiving nothing | both emulator bugs below produced exactly this. Check the container log for `bad XFGET` and `No data capability` |

**The two bugs behind that last symptom**, both fixed, both needing *two*
machines to show up at all:

- `b4ff6c9` — a received frame was booked as an XMSG allocation, so the next
  `XFGET` was refused forever. Signature: `bad XFGET, (Attach To Server), T reg
  = 0xffffffe5`.
- `80acc5e` — the XMSG rings were read through whichever domain was executing
  rather than the kernel's. Signature:
  `[MMU] TRAP: No data capability! domain=4 segment=6 vaddr=0x30000800`.

A single idle guest sits in domain 0 almost always and completes its attach
before any traffic exists, which is why one container never reproduced either.

## Clean up after yourself

```powershell
docker rm -f ndix1 ndix2 ; docker network rm ndixnet
```
```bash
sudo ~/repos/nd500x/tools/ndix-tap.sh down     # removes the nd0 device
```
