#!/bin/sh
#
# entrypoint.sh - bring up one containerised NDIX machine, end to end.
#
# What this does, in order:
#   1. pick the disk image (external mount wins, baked-in image otherwise)
#   2. derive the guest's system number, MAC and IP
#   3. build the container's network: TAP device + bridge with eth0
#   4. start nd500x with its console on a FIFO so we can type into the guest
#   5. wait for `login:`, log in as root, and run etconfig + ifconfig
#   6. hand the console over and stay in the foreground
#
# ===========================================================================
# WHY THE GUEST IS CONFIGURED OVER THE CONSOLE AND NOT IN THE DISK IMAGE
# ===========================================================================
#
# The obvious answer - write the address into the guest's /etc/rc - is not
# available. nd500x can READ a file out of an NDIX filesystem
# (src/frontend/nd500x/ndix_ffs.c, ndix_ffs_read_file) and there is NO WRITER.
# Nothing outside the emulator can put a file into the image.
#
# That turns out to be the better design anyway: one read-only 71 MB disk image
# serves any number of containers at any number of addresses. Baking the address
# in would mean a separate image per machine.
#
# So the address comes from environment variables and this script types
# etconfig + ifconfig at the guest console once it has booted.
#
# ===========================================================================
# THE NETWORK DESIGN, AND WHY IT IS THIS SHAPE
# ===========================================================================
#
# nd500x has exactly ONE uplink slot. nd500_xmsg_set_uplink() holds a single
# function pointer, so a machine is `tap` OR `tcp:host:port` OR `listen:port`
# - never two at once. That single fact decides everything below.
#
# The obvious design - TAP for the outside world plus a TCP link to the other
# guests - is therefore impossible. All traffic for one guest has to leave
# through one uplink, so the uplink has to be the one a normal Linux network can
# carry: `tap`.
#
# So each container gets its own TAP device and ROUTES between it and the
# container's ordinary eth0:
#
#      NDIX guest et0  <-->  nd0 (TAP, 223.255.254.201)
#                                 |
#                              routing + proxy ARP + DNAT
#                                 |
#      Docker / cluster net  <-->  eth0 (untouched, Docker's own address)
#
# The guest sits on its own segment, 223.255.254.0/24 by default - the historic
# ND range every existing note and script already uses. Because its netmask says
# the whole /24 is on-link, the guest ARPs for its peers, and the container
# answers those ARPs itself (proxy ARP) and routes the packet on to the peer's
# container. The guest therefore needs NO default route and NO gateway typed in
# at the console.
#
# Reaching a guest from OUTSIDE needs one more piece: Docker publishes ports to
# the CONTAINER's address and has never heard of the guest, so the container
# DNATs the published ports onward and SNATs them to its own address on the
# guest's wire. See setup_forwarding().
#
# WHY ROUTED AND NOT BRIDGED - THIS WAS MEASURED, NOT ASSUMED
# -----------------------------------------------------------
# Bridging the TAP together with eth0 is the tidier-looking design: the guest
# lands directly on the Docker network and two guests share one real ethernet
# segment, ARPing for each other with no routing at all. It was built that way
# first, and IT DOES NOT SURVIVE.
#
# What happens: bridging puts every broadcast and multicast frame on the Docker
# network in front of the guest - IPv6 router solicitations, other containers'
# ARP, and so on. After about eight such frames the guest's et0 stops receiving
# altogether. `netstat -i` freezes at Ipkts=8, the machine still transmits
# perfectly happily, and bringing the interface down and up again reports:
#
#     et0: bad XFGET, (Attach To Server), T reg = 0xffffffe5
#
# which is the XMSG sub-device hung - the failure mode
# docs/NDIX-NETWORKING.md lists under "sub-device hung, nothing printed". A
# 1988 driver that has never seen traffic it did not ask for is not the thing to
# harden here.
#
# Routing fixes it by construction: the only frames that ever reach the guest
# are the ones this container deliberately sends it. Nothing floods.
#
# It also happens to be the design Kubernetes needs. Pods are a layer-3 network
# and never share a layer-2 segment, so a bridged design could not have worked
# there at all.
#
# Rejected alternatives, so nobody has to rediscover why:
#   * bridging - see above. Measured to hang the guest.
#   * listen:/tcp: uplinks between guests - RETH framing, strictly point to
#     point, ONE peer each. Three guests would need a relay process, and the
#     host still could not telnet in because nothing outside the emulators
#     speaks RETH.
#   * macvlan - would give the guest a first-class address on the Docker net,
#     but it needs a parent interface with a real segment behind it and does not
#     work on Docker Desktop at all.
#
# ===========================================================================
# THE TRAPS. Every one of these has already cost somebody a day.
# ===========================================================================
#
#  * `-trailers` is MANDATORY on the ifconfig line. NDIX's ARP layer advertises
#    trailer encapsulation that its own `et` driver refuses to send, so without
#    the flag EVERY IP packet dies inside etoutput() with EPROTONOSUPPORT while
#    the machine looks perfectly healthy: et0 UP, ARP table full, counters
#    moving. See docs/NDIX-NETWORKING.md trap 1.
#  * etconfig MUST run BEFORE ifconfig. etinit() returns immediately unless the
#    interface has both an inet address and ET_SET (if/if_et.c:250-262).
#  * etconfig takes SIX SEPARATE BYTE ARGUMENTS. The `08:00:26:...` colon form
#    is rejected outright, and byte 0 must be even.
#  * root cannot log in over the NETWORK - /bin/login refuses root on a tty that
#    /etc/ttys does not mark secure, and prints only "Login incorrect". Telnet
#    in as the ordinary user `ndix` instead. Root works on the console, which is
#    what this script uses.
#  * this `ping` has no -c. It is `ping host [packetsize [count]]`.
#  * the console DROPS THE ODD BYTE, so every pattern matched below is a short
#    distinctive fragment (`ogin:`, not a whole line).
#  * the guest is SLOW: ~45 s to a login prompt, seconds per command, and slower
#    still when several containers boot at once. NOTHING here is a blind sleep
#    that hopes the machine kept up - every step waits for the prompt.
#
set -eu

log() { printf '[ndix-docker] %s\n' "$*" >&2; }
die() { log "ERROR: $*"; exit 1; }

RUNDIR=/run/ndix
CONSOLE_FIFO="$RUNDIR/console.in"
CONSOLE_OUT="$RUNDIR/console.out"
CONSOLE_LOG="$RUNDIR/console.log"
mkdir -p "$RUNDIR"

# ---------------------------------------------------------------------------
# 1. Which disk image?
# ---------------------------------------------------------------------------
#
# Priority: NDIX_DISK (explicit) > anything mounted at /data > the baked-in
# image. The /data scan takes the first *.img in name order, so a bind mount of
# a single file - `-v /my/rootfs.img:/data/rootfs.img` - just works.
BUILTIN_DISK=/opt/nd500x/disk/rootfs.img

if [ -n "${NDIX_DISK:-}" ]; then
    [ -f "$NDIX_DISK" ] || die "NDIX_DISK=$NDIX_DISK is not a file"
    DISK="$NDIX_DISK"
    DISK_SRC="NDIX_DISK"
else
    DISK=""
    for f in /data/*.img; do
        [ -f "$f" ] || continue
        DISK="$f"
        break
    done
    if [ -n "$DISK" ]; then
        DISK_SRC="mounted at /data"
    else
        DISK="$BUILTIN_DISK"
        DISK_SRC="baked into the image"
    fi
fi
[ -f "$DISK" ] || die "no NDIX disk image found (looked at /data/*.img and $BUILTIN_DISK)"
log "disk image : $DISK ($DISK_SRC)"

# ND500X_DISK_RW=1 is a COPY-ON-WRITE session: guest writes land in a session
# file and the master image is never touched. That is the right default for a
# container - a bind-mounted image survives `docker run` unharmed - and it is
# also nd500x's own default for --ndix (nd500x_ndix.c:222).
log "disk mode  : ND500X_DISK_RW=${ND500X_DISK_RW:-1} (1 = copy-on-write, master untouched)"

# ---------------------------------------------------------------------------
# 2. Identity: system number, MAC, IP
# ---------------------------------------------------------------------------
#
# NDIX_ID is a convenience knob that moves all three together, because every
# guest on a shared segment needs a unique MAC *and* a unique IP, and getting
# one of them wrong produces a fault that looks like something else entirely.
# NDIX_ID=1 reproduces the exact machine every existing note describes:
# system number 500, MAC 08:00:26:F4:01:00, address 223.255.254.8.
#
# Any of the three can also be set outright, and an explicit setting always
# wins over the NDIX_ID-derived default.
#
# KUBERNETES: a StatefulSet names its pods ndix-0, ndix-1, ... and that ordinal
# is the only per-pod identity the platform hands out for free. Turning it into
# NDIX_ID means one manifest, no per-pod ConfigMap, and an identity that is
# stable across a restart - which a Deployment's random pod name would not be.
# The +1 keeps pod ndix-0 as NDIX_ID=1, the canonical machine.
if [ -z "${NDIX_ID:-}" ]; then
    _ord=$(hostname 2>/dev/null | sed -n 's/.*-\([0-9][0-9]*\)$/\1/p')
    [ -n "$_ord" ] && NDIX_ID=$((_ord + 1))
fi
NDIX_ID="${NDIX_ID:-1}"
case "$NDIX_ID" in ''|*[!0-9]*) die "NDIX_ID must be a number, got '$NDIX_ID'" ;; esac
[ "$NDIX_ID" -ge 1 ] && [ "$NDIX_ID" -le 100 ] || die "NDIX_ID must be 1..100"

# The ND system number. It is also the high half of the XMSG magic number, so
# moving it keeps the MAC and the magic number consistent with each other.
NDIX_SYSNO="${NDIX_SYSNO:-$((499 + NDIX_ID))}"
export ND500X_SYSNO="$NDIX_SYSNO"

# MAC form is fixed by the hardware: 08:00:26:<sysno-lo>:<sysno-hi>:00.
# 500 = 0x01F4 -> 08:00:26:F4:01:00.
MAC_LO=$((NDIX_SYSNO & 255))
MAC_HI=$(((NDIX_SYSNO >> 8) & 255))
MAC_PRETTY="${NDIX_MAC:-$(printf '08:00:26:%02X:%02X:00' "$MAC_LO" "$MAC_HI")}"

# etconfig wants six separate byte arguments and REJECTS the colon form, so
# split whatever we ended up with back into bytes for it.
MAC_ARGS=$(echo "$MAC_PRETTY" | tr ':-' '  ' | awk '{for(i=1;i<=NF;i++) printf "0x%s ", $i}')

# The guest's address. Default subnet is the historic 223.255.254.0/24 that
# every existing NDIX note uses, with .8 as the first machine.
NDIX_IP="${NDIX_IP:-223.255.254.$((7 + NDIX_ID))}"
NDIX_NETMASK="${NDIX_NETMASK:-255.255.255.0}"

# The container's OWN address on the guests' subnet. Inbound connections are
# SNATed to this, so the guest always answers something on-link and never needs
# a route or a default gateway. One per container, derived from NDIX_ID so two
# containers cannot collide.
NDIX_HOST_IP="${NDIX_HOST_IP:-223.255.254.$((200 + NDIX_ID))}"
NDIX_GUEST_NET="${NDIX_GUEST_NET:-223.255.254.0/24}"

log "identity   : NDIX_ID=$NDIX_ID  sysno=$NDIX_SYSNO  MAC=$MAC_PRETTY"
log "guest addr : $NDIX_IP netmask $NDIX_NETMASK   (container side $NDIX_HOST_IP)"

# ---------------------------------------------------------------------------
# 3. Network: TAP + bridge
# ---------------------------------------------------------------------------
#
# nd500x does NOT create the TAP device and deliberately never will - making an
# interface and addressing it are root operations on host networking, and an
# emulator doing that silently is hard to undo. See tools/ndix-tap.sh. So the
# container does it, which is why it needs --cap-add=NET_ADMIN and
# --device=/dev/net/tun.
TAP_DEV="${NDIX_TAP_DEV:-nd0}"
CONTAINER_CIDR=""
CONTAINER_IP=""
GATE_CLOSED=0

# Let traffic through to the guest. Called once the guest has an address (or
# once we have given up trying to give it one - a machine somebody is going to
# configure by hand still needs its network to work afterwards).
gate_open() {
    [ "$GATE_CLOSED" = "1" ] || return 0
    iptables  -D OUTPUT  -o "$TAP_DEV" -j DROP 2>/dev/null || true
    iptables  -D FORWARD -o "$TAP_DEV" -j DROP 2>/dev/null || true
    ip6tables -D OUTPUT  -o "$TAP_DEV" -j DROP 2>/dev/null || true
    ip6tables -D FORWARD -o "$TAP_DEV" -j DROP 2>/dev/null || true
    GATE_CLOSED=0
    log "network    : gate open - the guest can now be reached"
}

setup_network() {
    [ -c /dev/net/tun ] || die "/dev/net/tun is missing. Run with --device=/dev/net/tun"

    CONTAINER_CIDR=$(ip -4 -o addr show dev eth0 2>/dev/null | awk '{print $4; exit}')
    [ -n "$CONTAINER_CIDR" ] || die "eth0 has no IPv4 address - is the container on a network?"
    CONTAINER_IP=${CONTAINER_CIDR%/*}

    # 'mode tap' = ethernet frames. 'mode tun' would be bare IP and NDIX would
    # never see a MAC header, so ARP - and therefore everything - would fail.
    ip tuntap add dev "$TAP_DEV" mode tap 2>/dev/null || true

    # SHUT THE GATE before the device carries anything.
    #
    # This is not belt and braces, it is the fix for a failure that was measured
    # twice. The guest's et0 hangs if it is handed frames before it has been
    # configured: `netstat -i` freezes at Ipkts around 8-10, transmits keep
    # climbing, ARP goes unanswered, and `ifconfig et0 down; up` then reports
    #     et0: bad XFGET, (Attach To Server)
    # It happened first when the TAP was bridged (so the whole Docker network's
    # broadcast traffic arrived during boot), and again with two containers
    # started together, where the FIRST machine's traffic reached the second
    # one's TAP while it was still booting. Both are the same thing: unsolicited
    # frames arriving before the driver is ready.
    #
    # So nothing reaches the guest until it has an address. gate_open() takes
    # these rules out once configuration is done.
    #
    # ip6tables matters as much as iptables here: the kernel sends an IPv6
    # router solicitation the moment the interface comes up, and that alone is a
    # frame the guest never asked for.
    iptables  -I OUTPUT  -o "$TAP_DEV" -j DROP
    iptables  -I FORWARD -o "$TAP_DEV" -j DROP
    ip6tables -I OUTPUT  -o "$TAP_DEV" -j DROP 2>/dev/null || true
    ip6tables -I FORWARD -o "$TAP_DEV" -j DROP 2>/dev/null || true
    GATE_CLOSED=1

    # The container's own address on the guests' segment. The TAP is a
    # point-to-point wire between the container and this one guest, so this
    # address is both "the other end of the wire" and the router the guest's
    # traffic leaves through.
    ip addr add "$NDIX_HOST_IP/${NDIX_GUEST_NET#*/}" dev "$TAP_DEV" 2>/dev/null || true
    ip link set "$TAP_DEV" up

    # eth0 is left exactly as Docker set it up. Nothing is moved, nothing is
    # bridged, the default route is untouched.
    log "network    : $TAP_DEV = $NDIX_HOST_IP (point to point to the guest)"
    log "             eth0 $CONTAINER_CIDR left alone"
}

# Peers: other NDIX guests, reachable through their own containers/pods.
#
# NDIX_PEERS is a list of "<guest-ip>@<container-or-pod-ip>", separated by
# spaces or commas:
#
#     NDIX_PEERS="223.255.254.9@172.30.0.12 223.255.254.10@172.30.0.13"
#
# For each peer we do two things:
#
#   * a HOST ROUTE to the peer guest via the machine that hosts it, so a packet
#     the guest sends actually gets somewhere;
#   * a PROXY ARP entry, so when the guest ARPs for a peer - which it will,
#     because its /24 says everyone is on-link - this container answers with its
#     own MAC and the guest hands the packet over to be routed.
#
# Proxy ARP is what removes the need for a default route inside the guest.
# Typing a route into a 1988 console over a link that drops the odd character is
# exactly the kind of step that fails once in ten and looks like a network
# fault, so the design avoids needing it at all.
#
# `ip neigh add proxy` is used rather than the net.ipv4.conf.*.proxy_arp sysctl
# because /proc/sys is READ-ONLY in an unprivileged container. Proxy neighbour
# entries are a netlink operation and work with NET_ADMIN alone - and they are
# tighter anyway: this answers for exactly the peers we were told about, not for
# every address that happens to be off-link.
#
# The next hop may be a NAME rather than an address:
#
#     NDIX_PEERS="223.255.254.9@ndix-1.ndix"
#
# which is what makes this usable on Kubernetes, where pod addresses are handed
# out fresh every time and only the StatefulSet's per-pod DNS name is stable.
# The name is resolved here and re-resolved periodically (see peer_refresh),
# because a pod that restarts comes back at a different address and a route
# pointing at the old one fails silently.
setup_peers() {
    [ -n "${NDIX_PEERS:-}" ] || return 0
    _quiet="${1:-}"

    for entry in $(echo "$NDIX_PEERS" | tr ',' ' '); do
        peer_ip=${entry%@*}
        via=${entry#*@}
        if [ "$peer_ip" = "$entry" ] || [ -z "$via" ]; then
            log "warning: NDIX_PEERS entry '$entry' is not <guest-ip>@<host>, skipped"
            continue
        fi

        # A bare name goes through the resolver; an address passes straight
        # through. `getent ahostsv4` is used rather than `host`/`dig` so the
        # image needs no DNS utilities.
        case "$via" in
            *[!0-9.]*)
                _resolved=$(getent ahostsv4 "$via" 2>/dev/null | awk '{print $1; exit}')
                if [ -z "$_resolved" ]; then
                    [ -n "$_quiet" ] || log "warning: cannot resolve peer host '$via' yet"
                    continue
                fi
                via="$_resolved"
                ;;
        esac

        ip route replace "$peer_ip/32" via "$via" 2>/dev/null || \
            log "warning: could not add a route to $peer_ip via $via"
        # Proxy ARP so the guest, which believes the whole /24 is on-link, gets
        # an answer and hands the packet to us to route.
        ip neigh replace proxy "$peer_ip" dev "$TAP_DEV" 2>/dev/null || \
            log "warning: could not proxy-ARP for $peer_ip"
        [ -n "$_quiet" ] || log "peer       : $peer_ip via $via (proxy ARP on $TAP_DEV)"
    done
}

# Re-resolve the peers every NDIX_PEER_REFRESH seconds. On Docker this is
# pointless and harmless; on Kubernetes it is the difference between a peer
# surviving a restart and disappearing for good. `ip route replace` is
# idempotent, so a re-run that changes nothing costs nothing.
peer_refresh() {
    [ -n "${NDIX_PEERS:-}" ] || return 0
    [ "${NDIX_PEER_REFRESH:-30}" -gt 0 ] 2>/dev/null || return 0
    while sleep "${NDIX_PEER_REFRESH:-30}"; do
        setup_peers quiet
    done
}

# Published container ports have to be carried on to the guest, because Docker
# DNATs them to the CONTAINER's Docker address and the guest lives elsewhere.
#
# Done in the nat table rather than with a userspace relay so the guest sees a
# real TCP connection with the right port numbers on both ends.
setup_forwarding() {
    # /proc/sys is mounted READ-ONLY in a container unless it is privileged, so
    # this write usually fails - and that is fine, because Docker already sets
    # net.ipv4.ip_forward=1 inside the network namespace. Check the VALUE, not
    # whether the write worked: refusing to start a container that already
    # forwards would be a self-inflicted outage.
    # The redirection is inside a subshell so that the SHELL's own "cannot
    # create: Read-only file system" message is suppressed too - redirecting
    # echo's stderr does not silence the shell reporting a failed redirect, and
    # a scary-looking error on every start is a support question waiting to
    # happen.
    if ! ( echo 1 > /proc/sys/net/ipv4/ip_forward ) 2>/dev/null; then
        if [ "$(cat /proc/sys/net/ipv4/ip_forward 2>/dev/null || echo 0)" = "1" ]; then
            log "ip_forward : already 1 (/proc/sys is read-only, which is normal)"
        else
            log "warning: ip_forward is 0 and /proc/sys is read-only. Published"
            log "         ports will not reach the guest. Add"
            log "         --sysctl net.ipv4.ip_forward=1 to the run command."
        fi
    fi

    for p in $(echo "${NDIX_FORWARD_PORTS:-23}" | tr ',' ' '); do
        case "$p" in ''|*[!0-9]*) log "warning: skipping non-numeric forward port '$p'"; continue ;; esac
        # Inbound from the Docker network / a published port.
        iptables -t nat -A PREROUTING -p tcp --dport "$p" \
                 -j DNAT --to-destination "$NDIX_IP:$p"
        # Locally generated too, so `docker exec ... nc localhost 23` and the
        # healthcheck reach the guest without knowing its address.
        iptables -t nat -A OUTPUT -p tcp -d "$CONTAINER_IP" --dport "$p" \
                 -j DNAT --to-destination "$NDIX_IP:$p"
        iptables -t nat -A OUTPUT -p tcp -d 127.0.0.1 --dport "$p" \
                 -j DNAT --to-destination "$NDIX_IP:$p"
        log "forward    : tcp/$p -> $NDIX_IP:$p"
    done

    # SNAT every inbound connection to the container's address on the guest's
    # own wire. The guest then only ever answers something on-link and needs no
    # route off its segment for inbound traffic at all.
    #
    # SNAT and not MASQUERADE: MASQUERADE picks the outgoing interface's first
    # address, and being explicit is one less thing that can quietly change.
    #
    # NOTE the exclusion: traffic arriving FROM a peer guest must NOT be SNATed,
    # or guest-to-guest ping would come back looking like it came from the
    # router and the reply would take a different path than the request.
    iptables -t nat -A POSTROUTING -d "$NDIX_IP" -o "$TAP_DEV" \
             ! -s "$NDIX_GUEST_NET" -j SNAT --to-source "$NDIX_HOST_IP"
}

if [ "${NDIX_SKIP_NETWORK:-0}" = "1" ]; then
    log "network    : skipped (NDIX_SKIP_NETWORK=1)"
else
    setup_network
    setup_peers
    setup_forwarding
    peer_refresh &
fi

# ---------------------------------------------------------------------------
# 4. Start the emulator with its console on a FIFO
# ---------------------------------------------------------------------------
#
# nd500x reads the guest console from stdin (nd_tty.c:390-403 - select() then
# read(), so a PIPE behaves exactly like a terminal; only the raw-mode setup is
# skipped when isatty() says no). Giving it a FIFO lets this script type into
# the guest while the container's own stdin still reaches it too.
#
# WHICH STREAM WE MATCH ON: plain stdout, the console itself. NOT
# ND500X_FEDBG=1 - that hex-dumps every fecall and would bury the console in
# debug traffic. nd500x flushes stdout after every character
# (nd500x_shell.c:163), so what we grep is never a stale block buffer, which is
# the only reason prompt matching is reliable at all.
#
# A named pipe was chosen over `expect`: it needs no extra language runtime in
# the image, and the SAME pipe then serves `docker attach` and the console-over-
# TCP bridge, so there is one console with several typists rather than three
# competing mechanisms.
rm -f "$CONSOLE_FIFO" "$CONSOLE_OUT"
mkfifo "$CONSOLE_FIFO"
mkfifo "$CONSOLE_OUT"
: > "$CONSOLE_LOG"

# What the healthcheck and the console bridge need, recorded once so neither has
# to re-derive it and get it subtly different.
cat > "$RUNDIR/env" <<EOF
NDIX_IP=$NDIX_IP
NDIX_HOST_IP=$NDIX_HOST_IP
CONSOLE_FIFO=$CONSOLE_FIFO
CONSOLE_LOG=$CONSOLE_LOG
MAC=$MAC_PRETTY
SYSNO=$NDIX_SYSNO
EOF

# ---------------------------------------------------------------------------
# 4a. Put the network configuration INTO the image, if we can
# ---------------------------------------------------------------------------
#
# Two ways to configure the guest:
#
#   console  (DEFAULT, and the one that works) wait for `login:`, log in as
#            root on the console, and type etconfig then ifconfig, waiting for
#            the shell prompt after each. Verified: telnet in from the host and
#            guest-to-guest ping both work after this.
#
#   rc       patch the two commands into /etc/rc inside a writable COPY of the
#            image, so NDIX configures et0 as part of its own boot.
#            DOES NOT WORK TODAY - see below. Kept because the plumbing is
#            correct and the finding is worth preserving.
#
# WHY `rc` IS NOT THE DEFAULT, EVEN THOUGH IT IS THE NICER DESIGN
# ---------------------------------------------------------------
# It was built and measured. tools/ndix/ndixput.py patches /etc/rc correctly,
# the block runs, and the guest prints exactly what you want to see:
#
#     ndix-docker: configuring et0
#     et0: flags=63<UP,BROADCAST,NOTRAILERS,RUNNING>
#             inet 223.255.254.8 netmask ffffff00 broadcast 223.255.254.255
#
# and then the interface DOES NOT WORK. It never answers an ARP request, the
# container's neighbour entry stays INCOMPLETE, and `netstat -i` in the guest
# shows receives frozen while transmits keep climbing - the same hang that
# bridging produced. Bringing et0 up LATE, from a shell after the machine has
# finished booting, works every time; bringing it up EARLY, from rc, does not.
# Why the two differ is NOT UNDERSTOOD and is a real finding to chase in the
# emulator, not something to paper over here.
#
# So: console by default. `NDIX_CONFIG_METHOD=rc` still does the patch if you
# want to reproduce the above.
#
# The image COPY is what makes the rc path safe: the image you mounted is NEVER
# written. It costs a 71 MB copy and a second or two at startup.
ETLINE="/etc/etconfig et0 $MAC_ARGS"
IFLINE="/etc/ifconfig et0 inet $NDIX_IP netmask $NDIX_NETMASK -trailers up"
CONFIG_METHOD="${NDIX_CONFIG_METHOD:-console}"
CONFIGURED_IN_IMAGE=0

if [ "${NDIX_AUTOCONF:-1}" = "1" ] && [ "$CONFIG_METHOD" = "rc" ]; then
    WORKIMG="${NDIX_WORK_DISK:-/run/ndix/rootfs.img}"
    log "config     : copying the disk image so /etc/rc can be patched (master untouched)"
    if cp "$DISK" "$WORKIMG" 2>/dev/null &&
       /opt/nd500x/bin/patch-rc.sh "$WORKIMG" "$ETLINE" "$IFLINE"; then
        DISK="$WORKIMG"
        CONFIGURED_IN_IMAGE=1
        log "config     : et0 will be configured by the guest's own /etc/rc"
    else
        rm -f "$WORKIMG"
        log "config     : could not patch /etc/rc, falling back to the console"
    fi
fi

export ND500X_ETH_UPLINK="${ND500X_ETH_UPLINK:-tap}"
export ND500X_NOXMSG="${ND500X_NOXMSG:-0}"
export ND500X_DISK_RW="${ND500X_DISK_RW:-1}"
export ND500X_CONSOLE_STDIN="${ND500X_CONSOLE_STDIN:-1}"

# The tap uplink defaults to device nd0; name it explicitly so a changed
# NDIX_TAP_DEV is actually honoured.
[ "$ND500X_ETH_UPLINK" = "tap" ] && export ND500X_ETH_UPLINK="tap:$TAP_DEV"

log "starting   : nd500x --ndix $DISK -N (uplink $ND500X_ETH_UPLINK)"

# Output goes through a second FIFO rather than a shell pipeline. In a pipeline
# `$!` is the pid of the LAST stage (tee), so `docker stop` would kill tee and
# leave the emulator running with nothing reading it. Splitting them gives a
# real pid for nd500x, which is what the TERM trap and `wait` below need.
tee -a "$CONSOLE_LOG" < "$CONSOLE_OUT" &
TEE_PID=$!

# shellcheck disable=SC2086
/opt/nd500x/bin/nd500x --ndix "$DISK" -N ${NDIX_EXTRA_ARGS:-} \
    < "$CONSOLE_FIFO" > "$CONSOLE_OUT" 2>&1 &
ND_PID=$!

# Open the FIFO read-write and hold it. A write-only open blocks until a reader
# appears, and every close would send EOF to the emulator's stdin, which it
# takes as "the operator went away".
exec 9<> "$CONSOLE_FIFO"

send() { printf '%s\n' "$1" >&9; }

# Match only output produced SINCE the last successful match. Matching the whole
# log would find a '#' in the boot messages and conclude the shell was ready.
LOGPOS=0
wait_for() {
    _pat="$1"; _secs="$2"; _i=0
    while [ "$_i" -lt "$_secs" ]; do
        if [ -s "$CONSOLE_LOG" ] && \
           tail -c "+$((LOGPOS + 1))" "$CONSOLE_LOG" 2>/dev/null | grep -q "$_pat"; then
            LOGPOS=$(wc -c < "$CONSOLE_LOG" | tr -d ' ')
            return 0
        fi
        # Has the emulator died? Then no prompt is ever coming.
        kill -0 "$ND_PID" 2>/dev/null || { log "emulator exited while waiting for '$_pat'"; return 2; }
        sleep 1
        _i=$((_i + 1))
    done
    log "timeout after ${_secs}s waiting for '$_pat'"
    return 1
}

# Type one command and wait for the shell prompt to come back. This is what
# makes the sequence safe under load: etconfig has genuinely FINISHED before
# ifconfig is sent, rather than being given a fixed number of seconds and hoped
# for. Trap 4: the order is not optional.
run_guest() {
    log "guest: $1"
    send "$1"
    wait_for '#' "${2:-60}" || { log "no prompt back after: $1"; return 1; }
}

# ---------------------------------------------------------------------------
# 5. Log in and configure et0
# ---------------------------------------------------------------------------
autoconfigure() {
    log "waiting for the guest to boot (~45 s idle, longer when several start at once)"
    # 'ogin:' not 'login:' - the console drops the odd byte (trap 8), and a
    # short distinctive fragment survives that where a whole line might not.
    wait_for 'ogin:' "${NDIX_BOOT_TIMEOUT:-300}" || return 1
    log "login prompt reached"

    # root, no password. This is the CONSOLE, where root is allowed; over the
    # network /bin/login refuses root and says only "Login incorrect".
    send 'root'
    wait_for '#' 120 || { log "no root shell prompt"; return 1; }
    log "root shell on the console"

    # ORDER MATTERS: etconfig sets ET_SET, and etinit() bails out without it.
    run_guest "/etc/etconfig et0 $MAC_ARGS" 120 || return 1

    # -trailers OR NOTHING WORKS. See the header of this file.
    run_guest "/etc/ifconfig et0 inet $NDIX_IP netmask $NDIX_NETMASK -trailers up" 120 || return 1

    # Read it back. NOTRAILERS in the flags is the proof that the one flag that
    # matters actually took. UP on its own proves nothing.
    log "guest: /etc/ifconfig et0"
    send '/etc/ifconfig et0'
    if wait_for 'NOTRAILERS' 60; then
        log "et0 is UP at $NDIX_IP with NOTRAILERS - the guest is on the network"
        return 0
    fi
    log "warning: could not confirm NOTRAILERS on et0 - check 'docker logs'."
    log "         The console drops the odd character, so this may be a false"
    log "         alarm. Try telnetting in before believing it."
    return 0
}

#
# When /etc/rc was patched there is nothing to type: the guest configures itself
# on the way up. All that is left is to WATCH for the proof. `NOTRAILERS` in the
# interface flags is that proof - UP on its own proves nothing, because an
# interface without the trailer flag looks perfectly healthy and drops every IP
# packet inside etoutput().
watch_selfconfig() {
    log "waiting for the guest to boot and configure et0 by itself"
    # /etc/rc runs BEFORE getty, and the block it now carries reads the
    # interface back itself - so the proof arrives on its own, before the login
    # prompt does. Nothing has to be typed at all.
    if wait_for 'NOTRAILERS' "${NDIX_BOOT_TIMEOUT:-300}"; then
        log "et0 is UP at $NDIX_IP with NOTRAILERS - the guest is on the network"
        return 0
    fi
    log "warning: could not confirm NOTRAILERS on et0 - check 'docker logs'."
    return 1
}

if [ "${NDIX_AUTOCONF:-1}" = "1" ] && [ "$CONFIGURED_IN_IMAGE" = "1" ]; then
    watch_selfconfig || log "self-configuration not confirmed; the console is still yours"
elif [ "${NDIX_AUTOCONF:-1}" = "1" ]; then
    autoconfigure || log "auto-config did not complete; the console is still yours"
else
    log "auto-config disabled (NDIX_AUTOCONF=0). Configure the guest yourself:"
    log "  /etc/etconfig et0 $MAC_ARGS"
    log "  /etc/ifconfig et0 inet $NDIX_IP netmask $NDIX_NETMASK -trailers up"
fi
gate_open
: > "$RUNDIR/ready"

# ---------------------------------------------------------------------------
# 6. Console access, then wait
# ---------------------------------------------------------------------------
#
# Optional console-over-TCP. Worth having: it works from the first boot message,
# before any networking exists - which is exactly when you need a console. It is
# also the only way in as ROOT, since the guest's own telnetd refuses root.
if [ -n "${NDIX_CONSOLE_PORT:-}" ] && [ "${NDIX_CONSOLE_PORT}" != "0" ]; then
    socat "TCP-LISTEN:${NDIX_CONSOLE_PORT},fork,reuseaddr" \
          "EXEC:/opt/nd500x/bin/console-bridge.sh" &
    log "console    : connect to port ${NDIX_CONSOLE_PORT} for the emulator console"
fi

# Relay the container's own stdin into the guest, so `docker attach` works.
# With `docker run -d` there is no stdin, cat sees EOF at once and exits - which
# is fine and must NOT bring the container down, hence the subshell.
( cat >&9 ) &

# Clean shutdown. SIGTERM from `docker stop` reaches the emulator instead of
# killing this script and orphaning it.
#
# NOTE: this stops the EMULATOR, it does not shut NDIX down cleanly. With the
# copy-on-write default that costs nothing, because the master image is never
# written. If you turn that off and persist writes some other way, halt the
# guest from its own console first - a 4.3BSD filesystem is only consistent once
# the kernel has flushed its buffer cache.
trap 'log "stopping"; kill "$ND_PID" 2>/dev/null || true' TERM INT

wait "$ND_PID"
STATUS=$?
kill "$TEE_PID" 2>/dev/null || true
log "nd500x exited with status $STATUS"
exit "$STATUS"
