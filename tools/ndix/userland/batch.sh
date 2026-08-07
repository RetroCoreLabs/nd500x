#!/bin/bash
# Build every userland program the image is missing.
#
# The list is the 59 names from the inventory pass: a program with source under
# baseline/{bin,ucb,usr.bin,etc} and no binary in the image. Single-file
# programs are built from <name>.c; directory programs from every .c in the
# directory, which is the usual shape for the bigger ones.
#
# Each result is reported, and failures are kept in the log rather than hidden -
# some of these will not build (missing headers, K&R quirks, programs that want
# curses or yacc output), and knowing WHICH is the point of the run.
set -u
NDIX=/mnt/e/Dev/Ronny/NDIX-C
SP=/home/ronny/repos/nd500x/work
PROG=/mnt/c/Users/ronny/AppData/Local/Temp/claude/E--Dev-Ronny-NDIX-C/8ae946ae-fd1d-44c4-a731-a036a9cf9531/scratchpad/prog.sh

NAMES="ac accton analyze badsect catman chown comsat diskpart dumpfs edquota
etconfig finger fingerd gettable mknod ncheck ndconfig nstat panalyze ping
pstat quotacheck quotaon reboot renice repquota rexecd rlogind rmail rmt
rshd sa savecore shutdown swapon telnet telnetd tftpd tftpsubs trpt
ftpd htable kanalyze named routed rwhod sendbug systat timed graph
conf config as ld mkproto"

mkdir -p "$SP/bin"
OK=""; FAILED=""
for n in $NAMES; do
    src=""
    for d in etc ucb bin usr.bin; do
        if [ -f "$NDIX/baseline/$d/$n.c" ]; then src="$NDIX/baseline/$d/$n.c"; break; fi
        if [ -d "$NDIX/baseline/$d/$n" ]; then
            files=$(ls "$NDIX/baseline/$d/$n"/*.c 2>/dev/null)
            if [ -n "$files" ]; then src="$files"; break; fi
        fi
    done
    if [ -z "$src" ]; then
        FAILED="$FAILED $n(nosrc)"; continue
    fi
    if out=$(bash "$PROG" "$n" $src 2>&1 | tail -1) && [ -s "$SP/bin/$n" ]; then
        OK="$OK $n"
        printf "  %-12s OK\n" "$n"
    else
        FAILED="$FAILED $n"
        printf "  %-12s %s\n" "$n" "$(echo "$out" | head -1)"
    fi
done

echo
echo "================================================"
echo "BUILT ($(echo $OK | wc -w)):"
echo "  $OK" | fold -sw 70 | sed 's/^/  /'
echo
echo "FAILED ($(echo $FAILED | wc -w)):"
echo "  $FAILED" | fold -sw 70 | sed 's/^/  /'
