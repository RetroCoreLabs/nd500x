#!/bin/sh
# stage-deb.sh - lay out the files the nd500x .deb installs
#
# Usage: tools/stage-deb.sh <build-dir> <stage-dir>
#
# Run from the repository root after `make release`. Writes into <stage-dir>
# (removed first):
#   usr/bin/nd500x                                    the binary
#   usr/share/applications/nd500x.desktop             menu entry
#   usr/share/icons/hicolor/scalable/apps/nd500x.svg  icon, any size
#   usr/share/icons/hicolor/512x512/apps/nd500x.png   icon, fixed size
#
# An ELF binary has no place for an icon; on Linux the icon belongs to the
# .desktop entry, which names it as "nd500x" and the desktop finds it in the
# hicolor theme. All three release jobs that build a .deb call this script so
# the packages hold the same files.
set -eu

BUILD="$1"
STAGE="$2"

rm -rf "$STAGE"
mkdir -p "$STAGE/usr/bin" \
         "$STAGE/usr/share/applications" \
         "$STAGE/usr/share/icons/hicolor/scalable/apps" \
         "$STAGE/usr/share/icons/hicolor/512x512/apps"

cp "$BUILD/bin/nd500x"                  "$STAGE/usr/bin/nd500x"
cp src/frontend/nd500x/nd500x.desktop   "$STAGE/usr/share/applications/nd500x.desktop"
cp assets/nd500x.svg                    "$STAGE/usr/share/icons/hicolor/scalable/apps/nd500x.svg"
cp assets/nd500x-512.png                "$STAGE/usr/share/icons/hicolor/512x512/apps/nd500x.png"
