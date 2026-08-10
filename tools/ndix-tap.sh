#!/bin/bash
#
# ndix-tap.sh - create, show or remove the Linux TAP device that puts NDIX's
#               et0 on the host network.
#
# Run this ONCE (it needs root). After that nd500x itself needs no privilege at
# all, because the device is created persistent and owned by your user.
#
#   sudo ./tools/ndix-tap.sh up          create nd0, 223.255.254.1/24, bring up
#   ./tools/ndix-tap.sh status           show the device and what is using it
#   sudo ./tools/ndix-tap.sh down        remove it again
#
# Then:
#   ND500X_ETH_UPLINK=tap ./build/bin/nd500x --ndix rootfs_net.img -N
#
# and in the guest:
#   /etc/etconfig et0 0x08 0x00 0x26 0xF4 0x01 0x00
#   /etc/ifconfig et0 inet 223.255.254.8 -trailers up
#
# then from the host:
#   ping 223.255.254.8
#   telnet 223.255.254.8
#
# WHY nd500x DOES NOT DO THIS ITSELF
# ---------------------------------
# Creating an interface, giving it an address and bringing it up are root
# operations that change host networking. An emulator doing that silently is
# hard to undo and easy not to notice, so nd500x only ever OPENS a device that
# already exists - and prints these commands if it cannot.
#
# -trailers IS NOT OPTIONAL, see docs/NDIX-NETWORKING.md: NDIX's ARP layer
# advertises trailer encapsulation that its own et driver refuses to send, and
# without the flag every IP packet dies inside etoutput with EPROTONOSUPPORT.

set -e

DEV="${NDIX_TAP_DEV:-nd0}"
HOST_IP="${NDIX_TAP_HOST_IP:-223.255.254.1}"
PREFIX="${NDIX_TAP_PREFIX:-24}"
GUEST_IP="${NDIX_TAP_GUEST_IP:-223.255.254.8}"

# The user who will own the device. When run under sudo, SUDO_USER is the human
# who typed it - using $USER there would hand the device to root and nd500x
# would then need root too, defeating the whole point.
OWNER="${SUDO_USER:-$USER}"

need_root() {
    if [ "$(id -u)" -ne 0 ]; then
        echo "ndix-tap: '$1' needs root. Try: sudo $0 $1" >&2
        exit 1
    fi
}

case "${1:-}" in

up)
    need_root up

    if [ ! -c /dev/net/tun ]; then
        echo "ndix-tap: /dev/net/tun is missing." >&2
        echo "          The kernel needs the 'tun' module: modprobe tun" >&2
        exit 1
    fi

    if ip link show "$DEV" > /dev/null 2>&1; then
        echo "ndix-tap: $DEV already exists, leaving it alone."
    else
        # 'mode tap' = ethernet frames (mode tun would be bare IP and NDIX
        # would never see a MAC header). 'user' makes it openable without root.
        ip tuntap add dev "$DEV" mode tap user "$OWNER"
        echo "ndix-tap: created $DEV owned by $OWNER"
    fi

    # Idempotent: adding an address that is already there is an error, so ask.
    if ip addr show "$DEV" | grep -q "inet $HOST_IP/"; then
        echo "ndix-tap: $DEV already has $HOST_IP/$PREFIX"
    else
        ip addr add "$HOST_IP/$PREFIX" dev "$DEV"
        echo "ndix-tap: $DEV given $HOST_IP/$PREFIX"
    fi

    ip link set "$DEV" up
    echo
    echo "ndix-tap: ready. The device shows NO-CARRIER until nd500x opens it -"
    echo "          that is normal, not a fault."
    echo
    echo "  ND500X_ETH_UPLINK=tap ./build/bin/nd500x --ndix <image> -N"
    echo
    echo "  guest:  /etc/etconfig et0 0x08 0x00 0x26 0xF4 0x01 0x00"
    echo "          /etc/ifconfig et0 inet $GUEST_IP -trailers up"
    echo "  host:   ping $GUEST_IP   /   telnet $GUEST_IP"
    ;;

down)
    need_root down
    if ip link show "$DEV" > /dev/null 2>&1; then
        ip tuntap del dev "$DEV" mode tap
        echo "ndix-tap: removed $DEV"
    else
        echo "ndix-tap: $DEV does not exist, nothing to remove."
    fi
    ;;

status)
    if ip link show "$DEV" > /dev/null 2>&1; then
        ip addr show "$DEV"
        echo
        # NO-CARRIER means no process has the tap open. That is the single most
        # useful thing to know, and the flag says it without needing lsof.
        if ip link show "$DEV" | grep -q NO-CARRIER; then
            echo "ndix-tap: NO-CARRIER - nothing has $DEV open (nd500x not running,"
            echo "          or not started with ND500X_ETH_UPLINK=tap)"
        else
            echo "ndix-tap: carrier up - something has $DEV open"
        fi
        echo
        echo "guest reachability:"
        ping -c 1 -W 2 "$GUEST_IP" > /dev/null 2>&1 &&
            echo "  $GUEST_IP responds" ||
            echo "  $GUEST_IP does not respond (guest down, or et0 not configured)"
    else
        echo "ndix-tap: $DEV does not exist. Create it with: sudo $0 up"
        exit 1
    fi
    ;;

*)
    echo "usage: $0 {up|down|status}"
    echo
    echo "  up      create $DEV, address $HOST_IP/$PREFIX, bring it up (root)"
    echo "  down    remove $DEV (root)"
    echo "  status  show $DEV, whether anything has it open, and if the guest answers"
    echo
    echo "override with NDIX_TAP_DEV, NDIX_TAP_HOST_IP, NDIX_TAP_PREFIX,"
    echo "NDIX_TAP_GUEST_IP."
    exit 1
    ;;
esac
