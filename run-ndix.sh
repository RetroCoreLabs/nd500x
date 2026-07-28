#!/bin/bash
#
# run-ndix.sh - Boot the NDIX kernel (Norsk Data 4.3BSD for ND-500) on nd500x.
#
# Usage: run-ndix.sh [-d <disk-image>] [-t <seconds>] [-l <logfile>] [-h]
#
# With stdin on a terminal this boots to the INTERACTIVE single-user shell:
# real /etc/init forks the real Bourne /bin/sh on /dev/console - type
# commands (e.g. "echo hello"); a line starting with '~' goes to the
# emulator debugger instead. End with Ctrl-D.
#
#   -d <image>    Root disk image (default: /mnt/e/Dev/Ronny/NDIX-C/rootfs_shell.img
#                 = real init + Bourne sh + /bin/echo + full /dev)
#                 Other images:
#                   /mnt/e/Dev/Ronny/NDIX-C/rootfs_hello.img (echo-test init)
#                   /mnt/e/Dev/Ronny/NDIX-C/rootfs_init.img  (stub init)
#                   /mnt/e/Dev/Ronny/NDIX-C/rootfs.img       (empty fs)
#   -t <seconds>  Run duration after 'run' is issued (default: 45).
#                 The outer timeout is <seconds> + 25.
#   -l <logfile>  Also tee output to <logfile>.
#                 NOTE: boot logs contain binary/control bytes - use 'grep -a'
#                 when searching the log file.
#   -h            Show this usage.
#
# Optional debug env vars (export before running, all default OFF):
#   ND500X_DOMDBG   domain switches + syscall returns with errno
#   ND500X_FEDBG    fecall/disk/console I/O
#   ND500X_PGFDBG   page faults
#   ND500X_SYSDBG   syscall path
#   ND500X_PATHDBG  namei path reads

set -u

KERNEL_DIR=/mnt/e/Dev/Ronny/NDIX-C/kernel/MASTER/GENERIC
SINTRAN_ROOT=/mnt/e/Dev/Ronny/NDIX-C
ND500X_BIN=/home/ronny/repos/nd500x/build/bin/nd500x

# Disk image: -d wins, then a pre-set ND500X_DISK, then the default.
DISK=${ND500X_DISK:-/mnt/e/Dev/Ronny/NDIX-C/rootfs_shell.img}
RUNTIME=45
LOGFILE=

usage() {
    sed -n '2,25p' "$0" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

while getopts "d:t:l:h" opt; do
    case $opt in
        d) DISK=$OPTARG ;;
        t) RUNTIME=$OPTARG ;;
        l) LOGFILE=$OPTARG ;;
        h) usage 0 ;;
        *) usage 1 ;;
    esac
done

if [ ! -x "$ND500X_BIN" ]; then
    echo "error: $ND500X_BIN not found" >&2
    echo "build it with: cmake --build /home/ronny/repos/nd500x/build --target nd500x -j4" >&2
    exit 1
fi

if [ ! -f "$DISK" ]; then
    echo "error: disk image not found: $DISK" >&2
    exit 1
fi

# cwd MUST be the kernel build dir: the debugger's 'load vmunix' resolves
# vmunix relative to cwd and auto-sources vmunix.init from there
# (mmusetup, load-pseg/load-dseg, THA/CTE1/CTE2/CAD register setup).
cd "$KERNEL_DIR" || exit 1

# ND500X_DISK is the disk-image env var (NDIX_DISK_IMAGE is silently ignored).
export ND500X_DISK="$DISK"
# Both REQUIRED: guest MMU routing; XMSG bypass (without NOXMSG proc0
# sleeps forever).
export ND500X_MMU_GUEST_TABLES=1
export ND500X_NOXMSG=1
# Lines typed while the machine runs go to the GUEST console (mx_bin input
# ring) instead of the debugger; prefix a line with '~' for the debugger.
export ND500X_CONSOLE_STDIN=1

TIMEOUT=$((RUNTIME + 25))

boot() {
    if [ -t 0 ]; then
        # Interactive: the boot commands come from a --script file, so stdin
        # stays connected to YOUR terminal - typed lines reach the NDIX shell
        # on /dev/console directly (no printf|cat pipe to die under us).
        # Ctrl-D no longer kills a running machine (the REPL lingers).
        BOOTCMDS=$(mktemp)
        printf 'load vmunix\nrun\n' > "$BOOTCMDS"
        "$ND500X_BIN" --debug --sintran-root "$SINTRAN_ROOT" --script "$BOOTCMDS"
        rm -f "$BOOTCMDS"
    else
        # Scripted: keep stdin open exactly RUNTIME seconds after 'run'.
        ( printf 'load vmunix\nrun\n'; sleep "$RUNTIME" ) | timeout "$TIMEOUT" \
            "$ND500X_BIN" --debug --sintran-root "$SINTRAN_ROOT"
    fi
}

if [ -n "$LOGFILE" ]; then
    boot 2>&1 | tee "$LOGFILE"
else
    boot
fi
