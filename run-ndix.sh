#!/bin/bash
#
# run-ndix.sh - thin wrapper around "nd500x --ndix".
#
# Everything this script used to do by hand (kernel directory, boot script,
# ND500X_* environment, cd into the kernel build dir) now lives inside nd500x
# itself. It is kept so existing callers keep working; new callers can just run
#
#     nd500x --ndix <root-disk-image> [--telnet]
#
# from any directory.
#
# Usage: run-ndix.sh [-d <disk-image>] [-k <kernel>] [-t <seconds>]
#                    [-l <logfile>] [-p <port>] [-n <ttys>] [-N] [-h]
#                    [<port> [<ttys>]]
#
# With stdin on a terminal this boots MULTIUSER to a login prompt: real
# /etc/init runs /etc/rc, then getty on /dev/console prints the banner and
# "login:". Log in as root (no password) to get the real Bourne /bin/sh - then
# type commands (e.g. "echo hello", "ls -l", "ps -ax"). A line starting with
# '~' goes to the emulator debugger instead. End with Ctrl-D.
#
# Press F12 at any time for the emulator menu: switch the terminal between the
# guest's virtual consoles, or shut NDIX down.
#
# Guest writes go THROUGH to the disk image and are still there next boot.
# Export ND500X_DISK_RW=cow for a scratch session that leaves the image alone,
# or ND500X_DISK_RW=0 for a read-only one.
#
# The guest terminals are ALSO served over telnet on port 5000 by default, so
# you can reach the same machine from another window with any telnet client:
#
#     telnet localhost 5000
#
# which offers a menu of the guest's terminals (console, tty01, tty02, tty81).
# The local terminal keeps working at the same time; console output is mirrored
# to both. Use -p to change the port, -n to offer fewer than all four terminals,
# or -N to not listen at all. The port and the tty count may also be given as
# plain arguments: "run-ndix.sh 5500" or "run-ndix.sh 5500 1".
#
#   -d <image>    Root disk image. Default: $ND500X_DISK, else $NDIX_ROOT/rootfs_full.img.
#   -k <kernel>   NDIX kernel image. Default: /vmunix read out of the disk image
#                 itself, so nothing else has to be shipped or found. Use -k to
#                 test a kernel you just rebuilt and have not copied into the
#                 image with nd500-mkproto yet.
#   -t <seconds>  Run duration when stdin is NOT a terminal (default: 45).
#                 The outer timeout is <seconds> + 25.
#   -l <logfile>  Also tee output to <logfile>.
#                 NOTE: boot logs contain binary/control bytes - use 'grep -a'
#                 when searching the log file.
#   -p <port>     Telnet port to listen on (default 5000).
#   -n <ttys>     How many guest terminals to offer, in the order console,
#                 tty01, tty02, tty81. Default: all 4, which is every terminal
#                 the shipped image has a /dev entry for.
#   -N            Do not start the telnet server.
#   -h            Show this usage.
#
# Environment:
#   NDIX_ROOT     Directory holding the NDIX tree (disk images + kernel/).
#                 Required unless -d gives a full path to the disk image.
#   ND500X_DISK, ND500X_DISK_RW, ND500X_MMU_GUEST_TABLES, ND500X_NOXMSG,
#   ND500X_CONSOLE_STDIN - still honoured; --ndix only sets defaults.
#
# Optional debug env vars (export before running, all default OFF):
#   ND500X_DOMDBG   domain switches + syscall returns with errno
#   ND500X_FEDBG    fecall/disk/console I/O
#   ND500X_PGFDBG   page faults
#   ND500X_SYSDBG   syscall path
#   ND500X_PATHDBG  namei path reads

set -u

# Repo root is derived from this script's own location - never hardcoded.
REPO_ROOT=$(cd "$(dirname "$0")" && pwd)
ND500X_BIN=${ND500X_BIN:-$REPO_ROOT/build/bin/nd500x}

# Machine-local settings live in an UNTRACKED file next to this script, so the
# repository itself stays free of absolute paths while a plain "./run-ndix.sh"
# still works on a machine that has been set up once. Create it with e.g.
#   echo 'NDIX_ROOT=/path/to/NDIX-C' > run-ndix.local
# Anything it sets (NDIX_ROOT, ND500X_DISK, ND500X_KERNEL, ...) acts as a
# default; a real environment variable or a command-line flag still wins.
if [ -f "$REPO_ROOT/run-ndix.local" ]; then
    # shellcheck disable=SC1090
    . "$REPO_ROOT/run-ndix.local"
fi

DISK=${ND500X_DISK:-}
KERNEL=
RUNTIME=45
LOGFILE=
# Telnet console is ON by default, port 5000, all four guest ttys.
TELNET=--telnet
TELNET_TTYS=

usage() {
    # The whole leading comment block IS the usage text, so take it to the last
    # comment line rather than a fixed number that goes stale on every edit.
    sed -n '2,/^[^#]/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

while getopts "d:k:t:l:p:n:Nh" opt; do
    case $opt in
        d) DISK=$OPTARG ;;
        k) KERNEL=$OPTARG ;;
        t) RUNTIME=$OPTARG ;;
        l) LOGFILE=$OPTARG ;;
        p) TELNET=--telnet=$OPTARG ;;
        n) TELNET_TTYS=$OPTARG ;;
        N) TELNET= ;;
        h) usage 0 ;;
        *) usage 1 ;;
    esac
done
shift $((OPTIND - 1))

# "./run-ndix.sh 5500" used to be accepted and thrown away: getopts stops at the
# first non-option, nothing looked at what was left, and the boot went up on the
# default port 5000 with no clue that the number had been ignored. A leading
# number is now the telnet port and an optional second one the tty count;
# anything else is an error rather than silence.
if [ $# -gt 0 ]; then
    case $1 in
        ''|*[!0-9]*) echo "error: unexpected argument '$1'" >&2; usage 1 ;;
    esac
    # -N asks for no telnet server and a port asks for one. Quietly letting the
    # port win would restart the listener the caller just switched off.
    if [ -z "$TELNET" ]; then
        echo "error: -N and a telnet port '$1' contradict each other" >&2
        usage 1
    fi
    TELNET=--telnet=$1
    shift
fi
if [ $# -gt 0 ]; then
    case $1 in
        ''|*[!0-9]*) echo "error: unexpected argument '$1'" >&2; usage 1 ;;
    esac
    TELNET_TTYS=$1
    shift
fi
if [ $# -gt 0 ]; then
    echo "error: unexpected argument '$1'" >&2
    usage 1
fi

if [ -z "$DISK" ]; then
    if [ -n "${NDIX_ROOT:-}" ]; then
        DISK=$NDIX_ROOT/rootfs_full.img
    else
        echo "error: no disk image. Use -d <image>, or export NDIX_ROOT (the" >&2
        echo "       directory holding the NDIX tree) or ND500X_DISK." >&2
        exit 1
    fi
fi

# No kernel hunting here any more. nd500x reads /vmunix out of the filesystem
# INSIDE the disk image, so the image is the only file that has to exist - which
# is the point of shipping one file. -k still overrides, and that is the switch
# to use when testing a kernel you just rebuilt and have not put in the image
# yet (nd500-mkproto is what puts it there).

if [ ! -x "$ND500X_BIN" ]; then
    echo "error: $ND500X_BIN not found" >&2
    echo "build it with: cmake --build $REPO_ROOT/build --target nd500x -j4" >&2
    exit 1
fi

if [ ! -f "$DISK" ]; then
    echo "error: disk image not found: $DISK" >&2
    exit 1
fi

ARGS=(--ndix "$DISK")
[ -n "$KERNEL" ] && ARGS+=(--kernel "$KERNEL")
[ -n "$TELNET" ] && ARGS+=("$TELNET")
# The tty count is a separate argv word and only means anything with -N absent.
[ -n "$TELNET" ] && [ -n "$TELNET_TTYS" ] && ARGS+=("$TELNET_TTYS")

boot() {
    if [ -t 0 ]; then
        # Interactive: stdin stays on YOUR terminal, so typed lines reach the
        # NDIX console directly.
        "$ND500X_BIN" "${ARGS[@]}"
    else
        # Scripted: hold stdin open exactly RUNTIME seconds, then let go.
        sleep "$RUNTIME" | timeout $((RUNTIME + 25)) "$ND500X_BIN" "${ARGS[@]}"
    fi
}

if [ -n "$LOGFILE" ]; then
    boot 2>&1 | tee "$LOGFILE"
else
    boot
fi
