#!/bin/sh
#
# healthcheck.sh - is the NDIX guest actually serving?
#
# "The process is alive" is a useless health signal here: nd500x stays alive
# perfectly happily with a guest that never finished booting, or with et0 down.
# So the test is the thing that matters - does the guest's telnetd answer?
#
# It answers as soon as et0 has an address, because /etc/rc starts /etc/inetd
# and /etc/inetd.conf already enables telnet. Nothing has to be started inside
# the guest.
#
# The start-period in the Dockerfile is 240 s because the guest needs ~45 s to
# reach a login prompt and the auto-config adds a good half minute on top.
#
set -eu

[ -f /run/ndix/env ] || { echo "not started yet"; exit 1; }
. /run/ndix/env

# -z is connect-and-close, -w is the timeout. This machine is slow, so 5 s.
if nc -z -w 5 "$NDIX_IP" 23; then
    echo "guest telnetd answers on $NDIX_IP:23"
    exit 0
fi

echo "no answer from $NDIX_IP:23"
exit 1
