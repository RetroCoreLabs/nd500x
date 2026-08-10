# NDIX — a 1988 Unix, in a container

This image runs **NDIX Release 3**, the Unix that Norsk Data shipped for its
ND-5000 minicomputers in 1988. It is a real 4.3BSD, not a re-creation: the same
kernel, the same `/bin/sh`, the same `login:` prompt. It runs on **nd500x**, an
emulator of the ND-500/5000 processor, and a bootable disk image is included, so
there is nothing to find and nothing to install.

Norsk Data was Norway's minicomputer company. It went under in the early 1990s
and most of its software went with it. This is one of the pieces that survived.

## Quick start

```sh
docker run --rm -it \
    --cap-add=NET_ADMIN --device=/dev/net/tun \
    -p 2301:23 -p 2401:2400 \
    ronnya/ndix-c:latest
```

Give it about a minute — a 1988 machine boots at 1988 speed. When the log says

```
[ndix-docker] et0 is UP at 223.255.254.8 with NOTRAILERS - the guest is on the network
```

you are on:

```sh
telnet localhost 2301        # the Unix machine.     log in as: ndix   (no password)
telnet localhost 2401        # the emulator console. log in as: root   (no password)
```

```
Release 3 NDIX (nd500x)
login: ndix
NDIX Release 3 on nd500x - Norsk Data ND-5000 emulator.
Welcome back to 1988.
$ /bin/hostname; /usr/ucb/whoami
nd500x
ndix
```

**Why the two capabilities?** The emulated machine's ethernet leaves through a
Linux TAP device, and the container has to create one. Without them the
container starts and stops with `/dev/net/tun is missing`. It is not
`--privileged` and does not need to be.

## Two machines that can talk to each other

```yaml
services:
  ndix1:
    image: ronnya/ndix-c:latest
    cap_add: [NET_ADMIN]
    devices: ["/dev/net/tun:/dev/net/tun"]
    environment:
      NDIX_ID: "1"                                  # guest 223.255.254.8
      NDIX_PEERS: "223.255.254.9@ndix2"
    ports: ["2301:23", "2401:2400"]
  ndix2:
    image: ronnya/ndix-c:latest
    cap_add: [NET_ADMIN]
    devices: ["/dev/net/tun:/dev/net/tun"]
    environment:
      NDIX_ID: "2"                                  # guest 223.255.254.9
      NDIX_PEERS: "223.255.254.8@ndix1"
    ports: ["2302:23", "2402:2400"]
```

Then, from inside machine 1:

```
$ /etc/ping 223.255.254.9
64 bytes from 223.255.254.9: icmp_seq=0. time=40. ms
```

That `ping` is from 1984 and **has no `-c`**. The usage is
`ping host [packetsize [count]]`. `ping -c 3 host` pings the literal string
`-c` and complains about the network for ever. Stop it with Ctrl-C.

## Environment variables

| Variable | Default | What it does |
|---|---|---|
| `NDIX_ID` | `1` | One knob for the machine's identity: system number, MAC and IP move together. **Give every container a different one.** |
| `NDIX_IP` | `223.255.254.(7+ID)` | The guest's address. |
| `NDIX_NETMASK` | `255.255.255.0` | The guest's netmask. |
| `NDIX_MAC` | derived | Full override, `08:00:26:F4:01:00` form. |
| `NDIX_SYSNO` | `499+ID` | ND system number; the MAC is derived from it. |
| `NDIX_PEERS` | — | Other guests: `<guest-ip>@<host>`, comma separated. The host may be a container name or a DNS name. |
| `NDIX_DISK` | — | Path to your own disk image inside the container. |
| `NDIX_FORWARD_PORTS` | `23` | TCP ports carried on to the guest. |
| `NDIX_CONSOLE_PORT` | `2400` | Port serving the emulator console. `0` disables. |
| `NDIX_AUTOCONF` | `1` | `0` leaves the guest's network unconfigured. |
| `NDIX_BOOT_TIMEOUT` | `300` | Seconds to wait for the guest to boot. |
| `ND500X_DISK_RW` | `1` | `1` = copy-on-write; the disk image is never written. |

## Your own disk image

Mount it at `/data` — anything ending `.img` there is used instead of the
built-in one:

```sh
docker run --rm -it --cap-add=NET_ADMIN --device=/dev/net/tun \
    -v /path/to/rootfs.img:/data/rootfs.img:ro \
    -p 2301:23 ronnya/ndix-c:latest
```

`:ro` is safe: guest writes go to a copy-on-write session inside the container
and your image is never touched.

Your image needs an ordinary user account. **Root cannot log in over the
network** — `/bin/login` refuses it on any tty that is not marked secure, and
says only `Login incorrect`, which looks exactly like a wrong password. The
built-in image has the account `ndix`. Root works on the console (port 2400).

## Things that will surprise you

- **Boot takes about 45 seconds** and each command takes seconds. That is not
  the container being slow.
- **`ping` has no `-c`.** See above.
- **No FTP.** The image ships `telnetd`, `rlogind`, `rshd` and `inetd` — and no
  `ftpd` binary at all, despite `/etc/inetd.conf` listing one. It also ships no
  `telnet` or `rlogin` *client*, so one guest cannot open a TCP connection to
  another. Guest-to-guest gets you ARP, IP and ICMP.
- **The console drops the odd character.** Never conclude a file is corrupt from
  reading it over the console.

## Tags

| Tag | What |
|---|---|
| `latest` | the current release |
| `1.0.0`, … | a specific release |
| `git-<sha>` | built from that exact nd500x commit |

Currently `linux/amd64` only.

## What is inside

Debian bookworm-slim, the `nd500x` emulator built from source, and a 71 MB NDIX
Release 3 disk image. About 170 MB in total.

Source, full documentation and the emulator itself:
<https://github.com/HackerCorpLabs/nd500x> — see `docker/README.md` for the
network architecture, Kubernetes manifests and a troubleshooting table.
