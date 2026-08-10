# NDIX networking on nd500x

How to get a 1988 ND-5000 running 4.3BSD onto a network you can reach, what
works today, and the traps that cost real time getting here.

Everything below is **measured on the machine**, not derived from the manuals.
Where something is unverified it says so.

---

## Quick start (Linux)

```sh
# once, as root - creates a persistent TAP owned by you
sudo ./tools/ndix-tap.sh up

# every run - no privilege needed after the step above
ND500X_ETH_UPLINK=tap ./build/bin/nd500x --ndix /path/to/rootfs_net.img -N
```

In the guest, at the `#` prompt:

```sh
/etc/etconfig et0 0x08 0x00 0x26 0xF4 0x01 0x00
/etc/ifconfig et0 inet 223.255.254.8 -trailers up
```

From the host:

```sh
ping 223.255.254.8
telnet 223.255.254.8          # log in as: ndix
./tools/ndix-tap.sh status    # is the device open? does the guest answer?
```

Measured result: ping 4/4 with no loss, and a shell over telnet.

```
Release 3 NDIX (nd500x)
login: ndix
NDIX Release 3 on nd500x - Norsk Data ND-5000 emulator.
Welcome back to 1988.
$ /bin/hostname; /usr/ucb/whoami
nd500x
ndix
```

---

## The uplinks

`ND500X_ETH_UPLINK` decides where et0's frames go. Exactly one uplink claims the
setting; they all register into the same one-function-pointer seam
(`nd500_xmsg_set_uplink`).

| Setting | What it does | Where it works |
|---|---|---|
| unset / `none` | frames dropped and counted | everywhere |
| `loop` | echoed straight back to the guest | everywhere, including wasm |
| `listen[:port]` | wait for one emulated peer to dial in (RETH framing) | native |
| `tcp:host[:port]` | dial an emulated peer or a RetroCore relay | native, Windows included |
| `tap[:dev]` | a real Linux TAP device, default `nd0` | **Linux only** |

`tap` is the only one the host can reach with ordinary tools. `listen`/`tcp`
speak RetroCore's RETH framing, which nothing outside the emulators understands.

**Windows has no TAP.** Use `tcp:` or `listen:` there. `uplink_tap.c` compiles on
Windows but says plainly that `/dev/net/tun` is a Linux interface rather than
failing in a way that looks like a configuration mistake.

---

## Why a TAP was necessary, not just convenient

The image ships TCP **servers** and no matching **client**:

| | present | absent |
|---|---|---|
| `/etc` | `telnetd`, `rlogind`, `rshd`, `rexecd`, `inetd` | `ftpd` (and `/etc/rc` says so itself) |
| `/usr/ucb` | `ftp`, `netstat`, `rdist` | `telnet`, `rlogin`, `arp` |

So one guest cannot open a TCP connection to another guest with anything on the
disk. Guest-to-guest proves ARP, IP and ICMP — and stops there. Reaching the
guest **from the host** is the only way to exercise TCP at all.

---

## Traps, all paid for in full

### 1. Trailers kill every IP packet between two NDIX machines

The single most expensive one. Symptom: `et0` UP, ARP table populated,
`netstat -i` counting both ways — and every IP packet fails with
`sendto: Protocol not supported`.

| Step | Where (NDIX source) | What happens |
|---|---|---|
| 1 | `netinet/if_ether.c:378-380, 387-391` | B answers an IP ARP request with a **second** reply carrying `arp_pro = ETHERTYPE_TRAIL` (0x1000) — "I accept trailers" — unless `IFF_NOTRAILERS` |
| 2 | `netinet/if_ether.c:329-332` | A receives it and sets `ATF_USETRAILERS` on its entry for B, **unconditionally** — A's own flag only gates *replying*, not accepting |
| 3 | `netinet/if_ether.c:197-198` | `arpresolve` returns `*usetrailers = 1` for every packet to B |
| 4 | `if/if_et.c:848-851` | `etoutput` refuses: `error = EPROTONOSUPPORT`, packet freed |

The ARP layer advertises trailer encapsulation that the `et` driver never
implemented. ARP itself survives because it leaves through the `AF_UNSPEC` arm of
`etoutput`, which has no trailer check — so the machine looks perfectly healthy.

**That second ARP reply is CORRECT 4.3BSD behaviour, not corruption.** Do not
"fix" it.

**Fix:** `-trailers` on **both** machines, before the address. One side is not
enough. `ifconfig` defers `SIOCSIFADDR` until all flags are set
(`baseline/etc/ifconfig.c:210-214`), so a single line is correctly ordered:

```sh
/etc/ifconfig et0 inet 223.255.254.8 -trailers up
```

Look for `flags=63<UP,BROADCAST,NOTRAILERS,RUNNING>`. Talking to a Linux host you
technically only need the guest flag, because Linux never sends trailer ARP — but
set it anyway so the same command works against another NDIX box.

**Do NOT fix this in nd500x.** Filtering `arp_pro = 0x1000` at the `*ENUM0`
server would be an emulator rewriting NDIX's protocol to hide an NDIX bug. The
wire was already correct; the host configuration was what was wrong.

### 2. `EPROTONOSUPPORT` from `sendto` is not a socket error

Grepping `sys/` alone finds it only in `socreate()` (`sys/uipc_socket.c:66`) and
sends you hunting the socket layer for nothing. It is also in the ethernet
driver. **Grep the whole kernel.**

### 3. Root cannot log in over the network, and the error does not say so

`/bin/login` refuses root on any tty `/etc/ttys` does not mark secure
(`baseline/bin/login.c:229`) and prints only `Login incorrect` — identical to a
wrong password. It also writes a `LOG_CRIT` "ROOT LOGIN REFUSED", but this image
has no syslog file, so nothing is recorded anywhere.

That refusal is correct and is exactly what the flag was invented for. The fix is
an ordinary account, which this image never had:

```sh
echo 'ndix::100:3:NDIX network user:/usr/ndix:/bin/sh' >> /etc/passwd
mkdir /usr/ndix
chown ndix /usr/ndix
```

`rootfs_net.img` already has this. **Empty password** — do not expose this
machine to a network you do not control.

**UNVERIFIED:** root logs in fine on the console even though `grep secure
/etc/ttys` finds nothing, which by the source above should refuse it there too.
I do not know why the two differ.

### 4. This `ping` has no `-c`

It is 4.2-era: `ping host [packetsize [count]]`. `ping -c 3 <host>` pings the
literal string `-c` and prints `Network is unreachable` forever, which reads
exactly like a routing fault.

### 5. The MAC address is not free

`08:00:26:<sysno-lo>:<sysno-hi>:00`. With `ND500X_SYSNO=500` (0x01F4) that is
`08:00:26:F4:01:00`. `etconfig` takes **six separate byte arguments**, decimal
unless `0x`-prefixed, and byte 0 must be even — `02:60:8c:...` colon form is
rejected outright.

### 6. `etconfig` must come before `ifconfig`

`etinit` returns immediately unless the interface has **both** an inet address
and `ET_SET` (`if/if_et.c:250-262`).

### 7. MTU is fixed at 1498

`if/if_et.c:190` sets `ETHERMTU - 2` and this vintage of `ifconfig` cannot lower
it. Fine on real ethernet; needs MSS clamping over anything smaller.

### 8. The console capture drops bytes

Reading a file through `ND500X_FEDBG` console output loses characters —
`root␉/etc/ftpd` came back as `root/ftpd`, and a whole `/etc/passwd` field
vanished. Never conclude a config file is malformed from a console read. Read the
file from `baseline/` on the host instead, where possible.

---

## What is proven, and what is not

| | Status |
|---|---|
| Two guests ARP over an emulated segment | proven |
| Two guests ping each other | proven — 82 replies, no loss |
| Host pings the guest over TAP | proven — 4/4 |
| inetd internal services (echo 7, daytime 13) | proven |
| telnetd: fork, exec, pty, login, shell | proven |
| `08:00:26:...` MAC through `etconfig` | proven |
| Against a real RetroCore relay | **never tested** |
| Windows-native build of the uplinks | **not yet re-verified** |
| wasm / WebSocket uplink | **not built** |

---

## Diagnostics

| Symptom | Look at |
|---|---|
| `et0` never appears | `ND500X_NOXMSG` must be 0; check `xgattach` in the boot log |
| `et0` exists but stays down | `etconfig` before `ifconfig`; both are required |
| ARP works, IP silent | trap 1 — `-trailers` on both ends |
| Nothing on the wire | `./tools/ndix-tap.sh status`; NO-CARRIER means nothing has the device open |
| Connection accepted then dropped | your client, most likely — `nc` closes stdin at once and telnetd exits with it |
| Sub-device wedged, nothing printed | the XMSG response's `func` must echo the request's, see `src/cpu/nd500_xmsg.h` |
| `bad XFGET, (Attach To Server), T reg = 0xffffffe5` | **fixed 2026-08-10**, see trap 9 below. If it reappears, the message-space bookkeeping has regressed |

### 9. A received frame is not an allocation (fixed, kept as a warning)

`0xffffffe5` is `-27` is `-033` octal is `XEXBF`, "message already has Xmsg
buffer" (`if/xmsg.h:153`).

A delivered receive makes its message the **current** message — it must, because
`if_et.c:422` and `:437` `XFWRI` the multicast request without an `XFGET` first.
But NDIX **never releases a received data frame**: `XFREL` appears at exactly two
places in the whole driver, `if_et.c:426` and `:462`, both inside the attach
handshake. The header comment at `if_et.c:629` claims `etrint` releases the
buffer and posts the next `XFRREN` — **the code does not do that.** Do not trust
that comment.

So a server that books a receive as an *allocation* can never have it released,
and refuses every later `XFGET`. The interface then comes up, transmits happily,
and receives nothing ever again.

**It only shows up with two machines.** Booting alone completes the attach before
any traffic exists. It needs a second machine already running whose ARP broadcast
lands mid-handshake — which is why it surfaced under Docker, where containers
start alongside each other, and never in the staggered two-guest tests. The same
root cause makes an `et0` configured early from `/etc/rc` come up and then never
answer ARP.

Frame counters are on stderr when the uplink closes: sent, received, dropped.
