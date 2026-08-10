#!/bin/sh
#
# console-bridge.sh - one TCP client <-> the emulator console.
#
# socat runs this once per connection (TCP-LISTEN:...,fork EXEC:this). stdin is
# the client's keystrokes, stdout is what the client sees.
#
# WHY THIS EXISTS ALONGSIDE THE GUEST'S OWN telnetd
# -------------------------------------------------
# The guest's telnetd only answers once et0 has an address. This bridge reaches
# the EMULATOR console, which works from the first boot message onward - so it
# is the thing you use when networking is what is broken. It is also where root
# can log in: /bin/login refuses root on any tty /etc/ttys does not mark secure,
# so over the guest's own telnet you are limited to the `ndix` account.
#
# It is NOT a telnet server - no IAC negotiation. `telnet host 2400` works
# because telnet falls back to a raw line-mode stream, and `nc host 2400` works
# outright. Local echo is off, so what you type appears when the guest echoes it.
#
set -eu

. /run/ndix/env

# The last 200 lines give a new client some context - usually the login prompt
# and whatever the auto-config printed - and then -f follows live output.
tail -n 200 -f "$CONSOLE_LOG" &
TAIL_PID=$!
trap 'kill "$TAIL_PID" 2>/dev/null || true' EXIT INT TERM

# Keystrokes go into the same FIFO the auto-config typed into, so this client
# and the container's own stdin share one console - exactly like two people at
# one terminal, which is what a single console really is.
cat > "$CONSOLE_FIFO"
