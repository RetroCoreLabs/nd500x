# NDIX in Docker

Run Norsk Data's **NDIX Release 3** — a real 4.3BSD Unix from 1988 — in a
container, on an emulated ND-5000, with working ethernet.

A disk image is baked in, so this needs no arguments:

```sh
docker run --rm -it \
    --cap-add=NET_ADMIN --device=/dev/net/tun \
    -p 2301:23 -p 2401:2400 \
    ronnya/ndix-c:latest
```

Give it about a minute. When the log says

```
[ndix-docker] et0 is UP at 223.255.254.8 with NOTRAILERS - the guest is on the network
```

the machine is on the network and you can telnet in:

```sh
telnet localhost 2301        # the NDIX guest.      log in as: ndix   (no password)
telnet localhost 2401        # the emulator console. root works here
```

---

## Contents

- [Why NET_ADMIN and /dev/net/tun](#why-net_admin-and-devnettun)
- [The network architecture, and why it is this shape](#the-network-architecture-and-why-it-is-this-shape)
- [Environment variables](#environment-variables)
- [Using your own disk image](#using-your-own-disk-image)
- [Several machines at once](#several-machines-at-once)
- [Adding a third machine](#adding-a-third-machine)
- [Kubernetes](#kubernetes)
- [Building and publishing](#building-and-publishing)
- [Troubleshooting](#troubleshooting)
- [What is in the image](#what-is-in-the-image)

---

## Why NET_ADMIN and /dev/net/tun

The guest's ethernet leaves the emulator through a Linux **TAP** device, and
`nd500x` only ever *opens* a TAP device that already exists — it never creates
one. That is deliberate, not an oversight: creating an interface, addressing it
and bringing it up are root operations on host networking, and an emulator doing
that silently is hard to undo and easy not to notice. `tools/ndix-tap.sh` says
the same thing at more length.

So the container's entrypoint creates the device, and needs the capability to do
it. Without those two flags the container starts and then stops with
`/dev/net/tun is missing`.

Nothing else needs privilege. The container is **not** `--privileged`.

---

## The network architecture, and why it is this shape

**The single fact that decides everything: nd500x has ONE uplink slot.**
`nd500_xmsg_set_uplink()` holds a single function pointer, so a machine is
`tap` **or** `tcp:host:port` **or** `listen:port` — never two at the same time.

That kills the obvious design. You cannot give a guest a TAP for the outside
world *and* a TCP link to its peers. All of one guest's traffic has to leave
through one uplink, so the uplink has to be the one that ordinary Linux
networking can carry: `tap`.

What the entrypoint therefore builds inside each container:

```
    NDIX guest et0  <-->  nd0 (TAP, 223.255.254.201)
                               |
                        routing + proxy ARP + DNAT
                               |
    Docker network  <-->  eth0 (untouched, Docker's own address)
```

The guest sits on its own segment — `223.255.254.0/24` by default, the historic
ND range every existing note and script already uses. Its netmask says the whole
/24 is on-link, so it ARPs for its peers, and the container **answers those ARPs
itself** and routes the packet on to the peer's container. The guest therefore
needs no default route and no gateway typed in at the console.

`eth0` is left exactly as Docker set it up. Nothing is moved and nothing is
bridged.

### Why routed and not bridged — this was measured

Bridging the TAP together with `eth0` is the tidier-looking design: the guest
lands directly on the Docker network, two guests share one real ethernet
segment, and they ARP for each other with no routing at all. **It was built that
way first and it does not survive.**

Bridging puts every broadcast and multicast frame on the Docker network in front
of the guest — IPv6 router solicitations, other containers' ARP, and so on.
After about eight such frames the guest's `et0` stops receiving altogether.
`netstat -i` freezes at `Ipkts=8`, the machine still transmits perfectly
happily, and bringing the interface down and up again reports:

```
et0: bad XFGET, (Attach To Server), T reg = 0xffffffe5
```

which is the XMSG sub-device wedged — the failure mode `docs/NDIX-NETWORKING.md`
lists under "sub-device wedged, nothing printed". Hardening a 1988 driver
against traffic it never asked for is not the job here.

Routing fixes it by construction: **the only frames that ever reach the guest are
the ones the container deliberately sends it.** Nothing floods.

### The gate

Routing alone is not quite enough. The same wedge appeared again with two
containers started together: the first machine's traffic reached the second
one's TAP while it was still booting, and that was enough — same symptoms,
receives frozen around `Ipkts=10`.

So the entrypoint **closes the TAP with an iptables DROP the moment it creates
it**, and only opens it once the guest has an address:

```
[ndix-docker] et0 is UP at 223.255.254.8 with NOTRAILERS - the guest is on the network
[ndix-docker] network    : gate open - the guest can now be reached
```

`ip6tables` gets the same treatment, because the kernel sends an IPv6 router
solicitation the instant the interface comes up and that on its own is a frame
the guest never asked for.

The underlying fragility is in the emulated XMSG path or the 1988 driver, not in
the container. Feeding a 1988 ethernet driver traffic before its interface
exists is not something anyone did in 1988, so it is not surprising that it
falls over — but it is worth chasing in `nd500x` rather than only being worked
around here.

It is also the only design that can work on Kubernetes, where pods are a layer-3
network and never share an ethernet segment.

### Reaching a guest from outside

Docker publishes ports to the **container's** address. It knows nothing about
the guest, which has a different address entirely. So the container DNATs each
published port onward:

```
host:2301 -> container 172.30.0.11:23 -> DNAT -> guest 223.255.254.8:23
                                      <- SNAT to 223.255.254.201
```

That is what `NDIX_FORWARD_PORTS` does — and it is a **list**, not just port 23.
Port 23 is the guest's own `telnetd`, already running: `/etc/rc` starts
`/etc/inetd` and `/etc/inetd.conf` already enables telnet, so nothing has to be
switched on inside the guest.

**A warning about anything other than telnet.** Port forwarding is address
translation, and protocols that carry addresses *inside* the conversation break
through it. Active-mode FTP is the classic case: the server opens a connection
back to the client on a port negotiated in-band, and NAT does not know to expect
it. A 1988 `ftpd` has no reliable passive-only mode. If you need protocols like
that, give the guest a directly reachable address instead of forwarding ports —
on a native Linux Docker host, add a route to `223.255.254.0/24` via the
container and talk to the guest's own address.

**As it happens, this is moot for FTP today.** `ls -l /etc/ftpd` in the guest
returns `/etc/ftpd not found`. `/etc/inetd.conf` lists `ftpd`, `talkd` and
`ntalkd`, and `/etc/rc` says in its own comments that they are not built. There
is no FTP server in this image to reach. `telnetd`, `rlogind`, `rshd` and
`fingerd` are present and do work.

On **native Linux** you can skip published ports entirely and route to the guest
directly. On Docker Desktop (Windows/macOS) the daemon lives in a VM and
published ports are the only way in.

### Alternatives that were rejected

| Idea | Why not |
|---|---|
| Bridging the TAP with `eth0` | Measured to wedge the guest's `et0` after ~8 unsolicited frames. See above. |
| `listen:` / `tcp:` uplinks between guests | RETH framing, strictly point to point, **one** peer each. Three guests would need a relay process that does not exist — and the host still could not telnet in, because nothing outside the emulators speaks RETH. |
| `macvlan` | Would give the guest a first-class address on the Docker network, but it needs a parent interface with a real segment behind it, and it does not work on Docker Desktop at all. |

---

## How the guest gets configured

The guest is configured by **typing at its console after it boots**.

There is a second method, and it is the one that *ought* to be better — patch
the two commands into the guest's `/etc/rc` so NDIX brings its own interface up
during boot. `tools/ndix/ndixput.py` can write a file back into an NDIX image in
place, so this is possible. It is implemented (`NDIX_CONFIG_METHOD=rc`,
`docker/patch-rc.sh`) and **it does not work**:

```
NDIX startup: /etc/rc running
ndix-docker: configuring et0
et0: flags=63<UP,BROADCAST,NOTRAILERS,RUNNING>
        inet 223.255.254.8 netmask ffffff00 broadcast 223.255.254.255
```

— exactly what you want to see, and then the interface never answers an ARP
request. The container's neighbour entry stays `INCOMPLETE`, and `netstat -i`
inside the guest shows receives frozen while transmits keep climbing. Bringing
`et0` up **late**, from a shell after the machine has finished booting, works
every time. Bringing it up **early**, from `rc`, does not. Why the two differ is
not understood, and it is a genuine thing to chase in the emulator rather than
work around here.

So: the console method, which is verified end to end. The entrypoint:

1. starts `nd500x` with its console on a **named pipe**, so the script can type
   into the guest while `docker attach` and the console-over-TCP bridge share the
   same console;
2. **waits for the `login:` prompt to appear** — it does not sleep a fixed number
   of seconds and hope. Boot is ~45 s idle and slower when several containers
   start at once, and a blind sleep gives you something that passes your test and
   races in production;
3. logs in as `root` on the console;
4. runs `etconfig`, **waits for the shell prompt to come back**, then runs
   `ifconfig`. The order is mandatory and so is the waiting;
5. reads the interface back and looks for `NOTRAILERS` in the flags, which is
   the proof the one flag that matters actually took.

A named pipe was chosen over `expect` because it needs no extra language runtime
in the image and because the same pipe then serves every other way of reaching
the console. The stream that gets pattern-matched is **plain stdout** — the
console itself. Not `ND500X_FEDBG=1`, which hex-dumps every fecall and would
bury the console in debug traffic. `nd500x` flushes stdout after every character
(`src/frontend/nd500x/nd500x_shell.c:163`), so what gets grepped is never stuck
behind a block buffer.

The two commands, for reference:

```sh
/etc/etconfig et0 0x08 0x00 0x26 0xF4 0x01 0x00
/etc/ifconfig et0 inet 223.255.254.8 netmask 255.255.255.0 -trailers up
```

Set `NDIX_AUTOCONF=0` to skip all of this and do it yourself at the console.

---

## Environment variables

### Identity and addressing

| Variable | Default | What it does |
|---|---|---|
| `NDIX_ID` | `1` | Convenience knob. Moves system number, MAC and IP together. `1` reproduces exactly the machine every existing note describes. Range 1–100. **Give every container a different one.** |
| `NDIX_SYSNO` | `499 + NDIX_ID` | The ND system number. Also the high half of the XMSG magic number, and the source of the MAC. |
| `NDIX_MAC` | derived from `NDIX_SYSNO` | Full override, `08:00:26:F4:01:00` form. The hardware form is `08:00:26:<sysno-lo>:<sysno-hi>:00` and byte 0 must be even. |
| `NDIX_IP` | `223.255.254.(7 + NDIX_ID)` | The guest's address. |
| `NDIX_NETMASK` | `255.255.255.0` | The guest's netmask. |
| `NDIX_HOST_IP` | `223.255.254.(200 + NDIX_ID)` | The container's own address on the guest segment. Inbound connections are SNATed to this. |
| `NDIX_GUEST_NET` | `223.255.254.0/24` | The guest segment, used for the prefix length of `NDIX_HOST_IP`. |
| `NDIX_PEERS` | — | The other guests: `<guest-ip>@<host>`, comma or space separated. The host may be an address, a container name, or a DNS name. **This is what makes guest-to-guest work.** |
| `NDIX_PEER_REFRESH` | `30` | Seconds between re-resolving peer names. Pointless on Docker, essential on Kubernetes where a restarted pod comes back at a new address. `0` disables. |

`NDIX_ID` is also derived automatically from a StatefulSet pod's ordinal:
`ndix-0` becomes `NDIX_ID=1`. That is what lets one Kubernetes manifest serve
every replica.

### Disk

| Variable | Default | What it does |
|---|---|---|
| `NDIX_DISK` | — | Explicit path to a disk image inside the container. Beats everything else. |
| `ND500X_DISK_RW` | `1` | `1` = copy-on-write session; **guest writes never touch the image**. `0` = read-only. |

With no `NDIX_DISK` the entrypoint takes the first `/data/*.img` it finds, and
falls back to the baked-in `/opt/nd500x/disk/rootfs.img`.

### Networking and behaviour

| Variable | Default | What it does |
|---|---|---|
| `NDIX_AUTOCONF` | `1` | `0` skips the console login and leaves the guest unconfigured. |
| `NDIX_CONFIG_METHOD` | `console` | `console` types the two commands at the guest after boot. `rc` patches them into the image's `/etc/rc` instead — implemented, and **does not work**; see above. |
| `NDIX_BOOT_TIMEOUT` | `300` | Seconds to wait for the `login:` prompt before giving up on auto-config. The container stays up either way. |
| `NDIX_FORWARD_PORTS` | `23` | Comma-separated TCP ports DNATed on to the guest. |
| `NDIX_CONSOLE_PORT` | `2400` | TCP port serving the emulator console. `0` disables it. |
| `NDIX_TAP_DEV` | `nd0` | TAP device name inside the container. |
| `NDIX_BRIDGE_DEV` | `ndbr0` | Bridge name inside the container. |
| `NDIX_SKIP_NETWORK` | `0` | `1` builds no network at all. For a console-only run. |
| `NDIX_EXTRA_ARGS` | — | Extra arguments appended to the `nd500x` command line. |
| `ND500X_ETH_UPLINK` | `tap` | Passed through to the emulator. Change it and you are choosing a different architecture — read the uplink table above first. |
| `ND500X_NOXMSG` | `0` | Must stay `0`. `1` makes `xgattach` give up and `et0` never appears. |

---

## Using your own disk image

Mount it at `/data`. Anything ending `.img` there wins over the baked-in image.

```sh
docker run --rm -it \
    --cap-add=NET_ADMIN --device=/dev/net/tun \
    -v /path/to/my_rootfs.img:/data/rootfs.img:ro \
    -p 2301:23 -p 2401:2400 \
    ronnya/ndix-c:latest
```

Or name it outright:

```sh
    -v /path/to/images:/data:ro -e NDIX_DISK=/data/experiment.img
```

`:ro` is safe because the default `ND500X_DISK_RW=1` is a copy-on-write session:
guest writes go to a session file inside the container and **the image you
mounted is never written**. That also means changes are lost when the container
goes away, which is usually what you want and occasionally not.

The image must have an ordinary user account. **Root cannot log in over the
network** — `/bin/login` refuses root on any tty `/etc/ttys` does not mark
secure, and prints only `Login incorrect`, which is indistinguishable from a
wrong password. The baked-in image has the account `ndix` with no password. To
add one to your own image, from the guest console as root:

```sh
echo 'ndix::100:3:NDIX network user:/usr/ndix:/bin/sh' >> /etc/passwd
mkdir /usr/ndix
chown ndix /usr/ndix
```

---

## Several machines at once

```sh
docker compose -f docker/docker-compose.yml up -d --build
```

That brings up two guests on one segment:

| | container | guest | MAC | telnet | console |
|---|---|---|---|---|---|
| `ndix1` | 172.30.0.11 | 223.255.254.8 | 08:00:26:F4:01:00 | `localhost:2301` | `localhost:2401` |
| `ndix2` | 172.30.0.12 | 223.255.254.9 | 08:00:26:F5:01:00 | `localhost:2302` | `localhost:2402` |

Prove they can see each other. `ping` needs a raw socket, so it needs **root**,
which means the console (`localhost:2402`), not telnet — over telnet as `ndix`
you get `ping: socket: Permission denied`, which is correct and not a network
fault. Measured, from ndix2's console:

```
# /etc/ping 223.255.254.8 56 5
PING 223.255.254.8: 56 data bytes
64 bytes from 223.255.254.8: icmp_seq=2. time=680. ms
64 bytes from 223.255.254.8: icmp_seq=3. time=40. ms
64 bytes from 223.255.254.8: icmp_seq=4. time=40. ms

----223.255.254.8 PING Statistics----
5 packets transmitted, 3 packets received, 40% packet loss
```

The first packet or two are normally lost while ARP resolves across two hops.
Run it again and it is 0%.

**This `ping` has no `-c`.** The usage is `ping host [packetsize [count]]`.
`ping -c 3 host` pings the literal string `-c` and prints "Network is
unreachable" for ever, which reads exactly like a routing fault. Stop it with
Ctrl-C.

What one guest **cannot** do is open a TCP connection to another: the image
ships the servers (`telnetd`, `rlogind`, `rshd`) and none of the clients — no
`telnet`, no `rlogin`. Guest-to-guest proves ARP, IP and ICMP and stops there.
Exercising TCP means connecting **from the host**.

---

## Adding a third machine

Copy a service block in `docker-compose.yml` and change four things:

```yaml
  ndix3:
    # ...identical to ndix2...
    container_name: ndix3
    environment:
      NDIX_ID: "3"                    # -> guest 223.255.254.10, MAC ...:F6:01:00
      NDIX_PEERS: "223.255.254.8@ndix1,223.255.254.9@ndix2"
    ports:
      - "2303:23"
      - "2403:2400"
    networks:
      ndixnet:
        ipv4_address: 172.30.0.13
```

`NDIX_ID` derives the system number, the MAC and the guest IP together, so
there is nothing else to keep in step — except `NDIX_PEERS`, which is a list of
*other* machines and so has to be extended on ndix1 and ndix2 as well.

---

## Kubernetes

Manifests are in `k8s/`:

```sh
kubectl apply -f k8s/
kubectl get pods -l app=ndix -w
telnet <any-node> 30231        # ndix-0's guest
telnet <any-node> 30241        # ndix-0's emulator console
```

**These manifests are NOT verified on a live cluster.** There is no Kubernetes
running on the machine they were written on — Docker Desktop's built-in cluster
is not enabled, and enabling it is not this work's call to make. They parse and
the object shapes are right; `kubectl apply --dry-run` could not be run because
this version of `kubectl` contacts the API server even for a client-side dry
run. Treat them as a starting point that has been thought through, not as
something that has been seen to work.

### The privileges it needs, and what to do if you cannot have them

Every NDIX pod needs **both**:

- `securityContext.capabilities.add: ["NET_ADMIN"]`
- a `hostPath` volume for `/dev/net/tun`

There is no way around them in this design. The guest's ethernet leaves the
emulator through a TAP device and `nd500x` only ever opens one that already
exists, so the pod has to create it.

**Many managed clusters forbid exactly this.** Under the `baseline` or
`restricted` Pod Security Standards, adding `NET_ADMIN` and mounting a host
device are both refused and the pod will not schedule. Options, in order:

1. a namespace labelled `pod-security.kubernetes.io/enforce: privileged`;
2. a cluster where you set the policy (k3s, kind, Docker Desktop);
3. no guest networking at all — `NDIX_AUTOCONF=0` and `NDIX_SKIP_NETWORK=1`. The
   machine boots and works over the console; it simply has no ethernet.

**A privilege-free variant does not exist and cannot be built from what is
here.** Every guest would have to use the `tcp:` uplink and dial a relay that
repeats frames between them — and no such relay exists, in nd500x or in
RetroCore. Even with one written, it would only buy guest-to-guest: nothing
outside the emulators speaks RETH framing, so reaching a guest from outside the
cluster would still need a TAP somewhere, and therefore still need the
capability — just concentrated in one relay pod instead of spread across all of
them. That is a real and worthwhile design; it is not one that can be claimed
without building it.

### The part that is most likely to bite on a real multi-node cluster

Guest-to-guest works here by routing packets **whose source and destination
addresses are `223.255.254.x`** from one pod to another. Those are not pod
addresses, and the CNI has never heard of them.

On a single node this is fine. On a real multi-node cluster it depends entirely
on your CNI: Calico with its default source-address checks, and anything else
that does anti-spoofing, will **drop** a packet leaving a pod with a source
address outside that pod's allocation. Cilium and Calico can both be told to
allow it (Calico: an `IPPool` or the `net.ipv4.conf.*.rp_filter` felix setting;
Cilium: native routing with the range excluded from masquerading), but that is
cluster configuration, not something a manifest can carry.

**This is untested.** If guests can reach the outside world but not each other
on your cluster, this is the first thing to look at.

### Which Service shape

`k8s/ndix-services.yaml` uses **one NodePort Service per machine**, pinned to a
single pod with the `statefulset.kubernetes.io/pod-name` label. One Service in
front of all of them would be wrong: these are distinct computers with distinct
disks, not replicas, and load-balancing across them would hand you a different
machine on every connection.

A `LoadBalancer` block is included, commented out. Do not uncomment it unless
your cluster can actually provision one — otherwise the Service sits in
`<pending>` for ever, which looks like a fault and is not one.

### Why a StatefulSet and not a Deployment

Each machine needs a stable identity: a unique ND system number, a MAC derived
from it, and a unique IP. A Deployment gives pods random names, so a restarted
pod would come back as a different machine. A StatefulSet names them `ndix-0`,
`ndix-1`, … and the entrypoint turns that ordinal into `NDIX_ID`, which derives
all three together. It also gives per-pod DNS names, which is what `NDIX_PEERS`
points at — pod IPs change on every restart, those names do not.

There is no `livenessProbe` on purpose. The guest takes minutes to become
useful and a restart costs another full boot, so a liveness probe would turn a
slow boot into a crash loop. Readiness is the right signal: keep the pod out of
service until its telnetd answers, and leave a sick machine running so you can
look at its console.

---

## Building and publishing

The build context is the **repository root**, not `docker/`, because the
Dockerfile compiles `nd500x` from source.

```sh
./docker/build.sh                          # ronnya/ndix-c:dev
TAG=1.0.0 ./docker/build.sh                # ronnya/ndix-c:1.0.0
NDIX_MASTER_IMAGE=/path/rootfs.img ./docker/build.sh
```

`build.sh` first copies the master disk image into `docker/disk/`, because the
NDIX images live outside this repository and `COPY` cannot reach outside the
build context. **It only ever reads the master.** `docker/disk/` is gitignored —
the image does not belong in git.

The build needs the git submodules checked out (`external/libdap`,
`external/ndmonlib`). `ndmonlib` is a private repository, so **the image can
only be built from a local working tree**, not from a fresh public clone. That
affects building, not running — anyone can `docker pull` and run the result.

To publish:

```sh
docker login
./docker/publish.sh 1.0.0 --dry-run       # build and tag, push nothing
./docker/publish.sh 1.0.0                 # build, tag, push
```

which tags `ronnya/ndix-c:1.0.0`, `:latest` and `:git-<sha>`. The `git-<sha>`
tag is what ties a running container back to the exact nd500x commit it was
built from.

### Architectures: amd64 only, and why

`linux/amd64` builds and runs. **`linux/arm64` does not build**, and the reason
is not understood.

Under `docker buildx --platform linux/arm64` (QEMU emulation on an amd64 host),
cmake's `file(GLOB)` in `external/ndmonlib/CMakeLists.txt:13` returns **0**
source files where the amd64 build finds 234, and the link then fails with a
few hundred `undefined reference to mon_*` errors. The files are definitely
present in the arm64 build: a probe image that runs the identical `COPY`
sequence and then globs the same directory reports `LS=234` and
`PROBE_GLOB=234`. Same context, same cmake, same base image — and inside the
real build it is 0.

So this is **not** a source incompatibility with arm64 that anyone has
identified. It may well build fine on a native arm64 machine; that has not been
tried, and until it has, claiming arm64 support would be a guess.

`MULTIARCH=1 ./docker/publish.sh 1.0.0` builds both platforms with buildx and is
left in place for when someone has a native arm64 builder to settle it. It will
fail today. Note that buildx cannot load a multi-platform image into the local
daemon, so that path pushes directly — test the single-arch image first.

**The Docker Hub overview page is `docker/DOCKERHUB.md`.** It is not uploaded by
`docker push`; paste it into the repository's description on Hub by hand.

---

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| Container exits with `/dev/net/tun is missing` | Add `--cap-add=NET_ADMIN --device=/dev/net/tun`. |
| `RTNETLINK answers: Operation not permitted` | `NET_ADMIN` is missing. |
| No `login:` after several minutes | Check `docker logs` for the boot messages. Raise `NDIX_BOOT_TIMEOUT`. A very loaded host really is this slow. |
| `et0` never appears in the guest | `ND500X_NOXMSG` must be `0`. Look for `xgattach` in the boot log. |
| `et0` exists but stays down | `etconfig` must run **before** `ifconfig`. Both are required. |
| Guests ARP fine, ping fails, `sendto: Protocol not supported` | The `-trailers` trap. NDIX's ARP advertises trailer encapsulation its own `et` driver refuses to send, so every IP packet dies inside `etoutput()`. The flag is required **on both machines**. Look for `NOTRAILERS` in `ifconfig et0`. |
| `telnet localhost 2301` connects then drops | Your client. `nc` closes stdin immediately and `telnetd` exits with it. Use a real telnet. |
| Telnet in as `root` says `Login incorrect` | Correct behaviour. Root cannot log in over the network. Use `ndix`, or the console on port 2400. |
| `ping -c 3 host` says "Network is unreachable" for ever | This `ping` has no `-c`. It is pinging the string `-c`. |
| Two guests cannot see each other | Is `NDIX_PEERS` set on **both**, each naming the other? Same Docker network? Different `NDIX_ID` — two guests with the same ID share a MAC and an IP. On Kubernetes, see the CNI note above. |
| `ping: socket: Permission denied` | `ping` needs a raw socket and therefore root. Use the console port, not telnet. |
| Guest stops receiving; `netstat -i` frozen, transmits still climbing | The XMSG sub-device has wedged. `ifconfig et0 down; ifconfig et0 up` then reports `bad XFGET, (Attach To Server)`. Restart the container. This is what unsolicited ethernet traffic does to this driver — do not bridge the TAP. |
| Published port reaches nothing | Check `docker logs` for the `ip_forward is 0` warning. If you see it, add `--sysctl net.ipv4.ip_forward=1`. |
| Guest config file looks corrupt when you `cat` it | The console drops the odd byte. Never conclude a file is malformed from a console read. |
| Healthcheck stays `starting` for minutes | By design: `start-period` is 240 s, because the guest needs ~45 s to boot and the auto-config adds more. |

Useful commands:

```sh
docker logs -f ndix1                       # the console, live
docker attach ndix1                        # type at the console (Ctrl-P Ctrl-Q to detach)
telnet localhost 2401                      # same console, over TCP
docker exec ndix1 cat /run/ndix/env        # what the entrypoint decided
docker exec ndix1 ip -br addr              # the container's bridge and TAP
docker exec ndix1 iptables -t nat -L -n    # the port forwarding
docker inspect --format '{{.State.Health.Status}}' ndix1
```

---

## What is in the image

| | |
|---|---|
| Base | `debian:bookworm-slim` |
| `/opt/nd500x/bin/nd500x` | the emulator, built from this source tree |
| `/opt/nd500x/disk/rootfs.img` | NDIX Release 3, 71 MB, with an `ndix` account added |
| Tools | `iproute2`, `iptables`, `socat`, `iputils-ping`, `netcat-openbsd` |

Debian and not Alpine because `src/frontend/nd500x/uplink_tap.c` includes
`<linux/if.h>` alongside the libc network headers, and on musl that pair
collides. glibc's headers guard against it. Patching the emulator to suit the
container was the worse trade.

The whole image is about 167 MB uncompressed, of which the disk image is 71 MB.
