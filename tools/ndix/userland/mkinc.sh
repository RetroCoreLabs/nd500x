#!/bin/bash
# Build the merged include tree the cross-build needs - the "$SP/init/inc" that
# build1.sh passes first and that was lost with the old scratch directory.
#
# No single tree is a complete /usr/include:
#   NDIX-B/usr.include            1988 userland headers (no signal.h, no sys/)
#   NDIX-C/kernel/MASTER/h        the kernel headers - this IS <sys/*>
#   NDIX-C/baseline/usr.5include  the System V set
#
# On a real BSD, /usr/include/sys is a LINK to the kernel's h directory, and so
# are machine, net and netinet. Each one missing does not merely fail its own
# include: nd500-cpp has /usr/include compiled in as a fallback, so it silently
# resolves to the HOST's glibc and feeds 2020s C to a K&R front end. Every
# "features.h: undefined control" in the build log was that.
#
# INCLUDE GUARDS ARE ADDED HERE. The 1988 headers have none - signal.h wraps
# only its SCCS string in "#ifndef lint" - so a program that reaches the same
# header twice gets "redeclaration of NDIX_SVsignal_sccsid" and fails to
# compile. That blocked edquota, quotacheck, rexecd, rshd and telnetd among
# others. Guarding is done on COPIES in this build tree; not one byte of the
# preserved sources is touched.
set -u
NDIX=/mnt/e/Dev/Ronny/NDIX-C
NDIX_B=/mnt/e/Dev/Ronny/NDIX-B
INC=/home/ronny/repos/nd500x/work/init/inc

rm -rf "$INC"
mkdir -p "$INC"

# Copy one header, wrapped in a guard derived from its path.
guard_copy() {
    src=$1; dst=$2
    mkdir -p "$(dirname "$dst")"
    g="_NDX_$(echo "${dst#$INC/}" | tr './-' '___' | tr '[:lower:]' '[:upper:]')"
    { echo "#ifndef $g"; echo "#define $g"; cat "$src"; echo "#endif /* $g */"; } > "$dst"
}

copy_tree() {
    from=$1; to=$2; overwrite=$3
    [ -d "$from" ] || return 0
    find "$from" -maxdepth 1 -type f -name '*.h' | while read -r f; do
        d="$to/$(basename "$f")"
        [ "$overwrite" = no ] && [ -e "$d" ] && continue
        guard_copy "$f" "$d"
    done
    # one level of subdirectories (arpa/, protocols/, ...)
    find "$from" -mindepth 1 -maxdepth 1 -type d ! -name SCCS | while read -r sub; do
        find "$sub" -maxdepth 1 -type f -name '*.h' | while read -r f; do
            d="$to/$(basename "$sub")/$(basename "$f")"
            [ "$overwrite" = no ] && [ -e "$d" ] && continue
            guard_copy "$f" "$d"
        done
    done
}

# 1. the 1988 userland headers win
copy_tree "$NDIX_B/usr.include" "$INC" yes
# 2. the System V set fills gaps only
copy_tree "$NDIX/baseline/usr.5include" "$INC" no
# 3. the kernel's directories, under the names the sources use
copy_tree "$NDIX/kernel/MASTER/h"       "$INC/sys"     yes
copy_tree "$NDIX/kernel/MASTER/machine" "$INC/machine" yes
copy_tree "$NDIX/kernel/MASTER/net"     "$INC/net"     yes
copy_tree "$NDIX/kernel/MASTER/netinet" "$INC/netinet" yes
# The device-driver headers. etconfig, kanalyze and pstat include <if/xbuf.h>
# and <if/if_etioctl.h> - the Ethernet driver's own interface - so this tree
# needs to be reachable under the name "if" as well.
copy_tree "$NDIX/kernel/MASTER/if"      "$INC/if"      yes
# 4. the 4.3BSD overlay, applied LAST so it wins.
#
# NDIX-C is 4.3BSD-derived and its Release 3 libc includes <resolv.h> and
# <arpa/nameser.h>, which the archive never preserved - so they come from the
# real 4.3 tree instead of being reconstructed. netdb.h is deliberately
# OVERWRITTEN: the archive's copy is "netdb.h 2.5 87/05/13" and still has the
# 4.2-era "char *h_addr", while the libc shipped beside it
# (gethostnamadr.c 3.2 88/05/11) assigns host.h_addr_list. The header is simply
# older than the library. The 4.3 header is a superset - it keeps h_addr as
# "#define h_addr h_addr_list[0]" - so nothing that compiled before stops
# compiling. See bsd43/README.md for provenance and for what was verified
# against the NDIX-C sources before trusting these.
copy_tree "$(dirname "$0")/bsd43"       "$INC"         yes

echo "include tree: $INC"
echo "  headers: $(find "$INC" -name '*.h' | wc -l)"
for h in sys/types.h sys/socket.h sys/param.h machine/param.h netinet/in.h \
         net/if.h arpa/inet.h netdb.h signal.h stdio.h \
         resolv.h arpa/nameser.h ttyent.h curses.h; do
    printf "  %-18s %s\n" "$h" "$([ -e "$INC/$h" ] && echo present || echo MISSING)"
done
