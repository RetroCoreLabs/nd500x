#!/bin/bash
# Machine-specific roots come from the environment, never from the file.
#   NDIX      NDIX-C checkout (disk images + kernel/)
#   NDIX_B    NDIX-B checkout (authentic 1988 headers/sources)
#   PCC_ND500 pcc-nd500 checkout (the cross toolchain, its bin/)
# The nd500x repo root is derived from this script's own location.
REPO_ROOT=$(cd "$(dirname "$0")/../.." && pwd)
: "${NDIX:?set NDIX to your NDIX-C checkout}"

# stage.sh - build the host staging trees for the new NDIX disk image.
#   $SP/stage/root -> partition a      $SP/stage/usr -> partition e
set -e
# Scratch/work area. Override with ND500X_WORK; defaults inside the repo.
SP="${ND500X_WORK:-$REPO_ROOT/work}"
mkdir -p "$SP"
B=${PCC_ND500:?set PCC_ND500}/bin
NB=${NDIX_B:?set NDIX_B}
NC=$NDIX
R=$SP/stage/root
U=$SP/stage/usr
rm -rf $SP/stage
mkdir -p $R/etc $R/bin $R/lib $R/tmp $R/usr
mkdir -p $U/bin $U/ucb $U/lib $U/tmp $U/include

# ---- root ----
cp -L $NC/kernel/MASTER/GENERIC/vmunix $R/vmunix
cp -L $SP/init/init          $R/etc/init
cp -L $SP/etc/rc             $R/etc/rc
cp -L $SP/etc/passwd         $R/etc/passwd
cp -L $SP/etc/group          $R/etc/group
cp -L $SP/etc/ttys           $R/etc/ttys
cp -L $SP/etc/fstab          $R/etc/fstab
cp -L $SP/getty/getty        $R/etc/getty
cp -L $SP/etc/gettytab       $R/etc/gettytab
cp -L $SP/motd.txt           $R/etc/motd
cp -L $SP/usrbuild/mount     $R/etc/mount
cp -L $SP/usrbuild/umount    $R/etc/umount
chmod 755 $R/etc/init $R/etc/rc $R/etc/getty $R/etc/mount $R/etc/umount
chmod 644 $R/etc/passwd $R/etc/group $R/etc/ttys $R/etc/fstab $R/etc/gettytab $R/etc/motd $R/vmunix

cp -L $SP/sh/sh $R/bin/sh
for f in $SP/utils/*; do
    [ -f "$f" ] || continue
    cp -L "$f" $R/bin/$(basename "$f")
done
chmod 755 $R/bin/*

# native toolchain: the C driver compiled into it looks for exactly these paths
for f in cc1 cc2 cpp as ld; do cp -L $SP/nat/bin/$f $R/lib/$f; done
cp -L $SP/nat/lib/crt0.o  $R/lib/crt0.o
cp -L $SP/nat/lib/libc.a  $R/lib/libc.a
chmod 755 $R/lib/cc1 $R/lib/cc2 $R/lib/cpp $R/lib/as $R/lib/ld
chmod 644 $R/lib/crt0.o $R/lib/libc.a

# ---- /usr ----
for f in $SP/usrbuild/*; do
    bn=$(basename "$f")
    [ -f "$f" ] || continue
    case "$bn" in *.o|*.err|*.i|*.ic|*.s|mount|umount) continue;; esac
    cp -L "$f" $U/bin/$bn 2>/dev/null || true
done
cp -L $SP/nat/bin/cc $U/bin/cc
cp -L $SP/nat/lib/libm.a $U/lib/libm.a

# headers: the 1988 user headers plus the kernel's <sys/...>, <machine/...>,
# <net/...>, <netinet/...>, <protocols/...>
cp -L $NB/usr.include/*.h $U/include/ 2>/dev/null || true
cp -rL $NB/usr.include/arpa $U/include/arpa
mkdir -p $U/include/sys $U/include/machine $U/include/net $U/include/netinet $U/include/protocols
for d in sys:$NC/kernel/MASTER/h machine:$NC/kernel/MASTER/machine \
         net:$NC/kernel/MASTER/net netinet:$NC/kernel/MASTER/netinet; do
    t=${d%%:*}; s=${d#*:}
    for h in $s/*.h; do [ -f "$h" ] && cp -L "$h" $U/include/$t/$(basename $h); done
done
cp -L $SP/init/inc/protocols/*.h $U/include/protocols/ 2>/dev/null || true
cp -L $SP/init/inc/ttyent.h $U/include/ttyent.h 2>/dev/null || true
# errno.h / signal.h / setjmp.h that the cross build actually used
cp -L $SP/init/inc/errno.h  $U/include/errno.h
cp -L $SP/init/inc/signal.h $U/include/signal.h
cp -L $SP/init/inc/setjmp.h $U/include/setjmp.h
cp -L $SP/init/inc/netdb.h  $U/include/netdb.h
cp -L $SP/init/inc/utmp.h   $U/include/utmp.h
find $U/include -name "*~" -delete
chmod -R 644 $U/include
find $U/include -type d -exec chmod 755 {} +
chmod 755 $U/bin/* $U/ucb/* 2>/dev/null || true
echo "staged."
du -sh $R $U
