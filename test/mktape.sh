#!/bin/bash
# mktape.sh - build a SIMH .tap image for exercising the tape device (generic 2).
#
# Pure shell on purpose: anything on the build/test path must not need Python.
#
# SIMH .tap layout, which is what src/cpu/nd500_fecall.c reads:
#   record    <4-byte LE length N> <N data bytes, padded to even> <4-byte LE N>
#   N == 0            tape mark (filemark)
#   N == 0xFFFFFFFF   end of medium
#
# Usage: mktape.sh <out.tap> [record ...]
#   With no records given, writes a default layout of two files:
#       "hello tape", "second record", filemark, "file two", filemark, EOM
#
# Every length word is emitted little-endian regardless of host byte order,
# because the format defines it that way - it is NOT the guest's big-endian.
set -eu

out=${1:?usage: mktape.sh <out.tap> [record ...]}
shift || true

# Emit a 32-bit little-endian value as raw bytes.
le32() {
    local v=$1
    printf "$(printf '\\x%02x\\x%02x\\x%02x\\x%02x' \
        $(( v        & 0xFF )) \
        $(( (v >> 8)  & 0xFF )) \
        $(( (v >> 16) & 0xFF )) \
        $(( (v >> 24) & 0xFF )) )"
}

# Emit one data record: header, payload, even pad, trailer.
rec() {
    local data=$1
    local n=${#data}
    le32 "$n"
    printf '%s' "$data"
    [ $(( n % 2 )) -eq 1 ] && printf '\0' || true
    le32 "$n"
}

mark() { le32 0; }                 # filemark
eom()  { le32 4294967295; }        # 0xFFFFFFFF

{
    if [ $# -gt 0 ]; then
        for r in "$@"; do rec "$r"; done
        mark
    else
        rec "hello tape"
        rec "second record"
        mark
        rec "file two"
        mark
    fi
    eom
} > "$out"

echo "wrote $out ($(wc -c < "$out") bytes)"
