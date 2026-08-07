#!/bin/bash
# Build the whole NDIX libc from source and index it, rebuilding the work tree
# the cross-build needs.
#
# linkclose3.sh resolves undefined symbols by searching, in this order:
#   syslib genlib stdiolib netlib inetlib hostlib libm compat-4.1lib compat-sys5lib
# against $SP/libx/symindex.txt, whose lines are "<symbol> <lib>/<member.o>".
# Those names map one-for-one onto baseline/lib/libc's subdirectories, which is
# how the original work tree was built - and why it can be rebuilt from source.
#
# Three header roots, because none is complete on its own:
#   NDIX-B/usr.include            1988 userland headers (no signal.h)
#   NDIX-C/kernel/MASTER/h        kernel headers (signal.h is here)
#   NDIX-C/baseline/usr.5include  the System V set
# nd500-cpp has /usr/include compiled in as a fallback, so a header missing from
# all three resolves to the HOST's glibc and feeds 2020s C to a K&R front end.
set -u
export NDIX=/mnt/e/Dev/Ronny/NDIX-C
export NDIX_B=/mnt/e/Dev/Ronny/NDIX-B
export PCC_ND500=/home/ronny/repos/ragge/pcc-nd500
export ND500X_WORK=/home/ronny/repos/nd500x/work

B=$PCC_ND500/bin
L=$NDIX/baseline/lib/libc
W=$ND500X_WORK/libx
INC="-I/home/ronny/repos/nd500x/work/init/inc"

# source dir -> library name linkclose3.sh searches for
map_lib() {
    case "$1" in
        net/named)    echo hostlib ;;
        sys)          echo syslib ;;
        gen)          echo genlib ;;
        stdio)        echo stdiolib ;;
        net)          echo netlib ;;
        inet)         echo inetlib ;;
        compat-4.1)   echo compat-4.1lib ;;
        compat-sys5)  echo compat-sys5lib ;;
        *)            echo "$1"lib ;;
    esac
}

mkdir -p "$W"
: > "$W/build.log"
TOT_OK=0; TOT_FAIL=0

for d in sys gen stdio net net/named inet compat-4.1 compat-sys5; do
    lib=$(map_lib "$d")
    out="$W/$lib"
    mkdir -p "$out"
    ok=0; fail=0

    for src in "$L/$d"/*.c; do
        [ -e "$src" ] || continue
        bn=$(basename "$src" .c)
        if $B/nd500-cpp $INC "$src" > "$out/$bn.i" 2>>"$W/build.log" \
           && $B/nd500-cc1 < "$out/$bn.i" > "$out/$bn.ic" 2>>"$W/build.log" \
           && $B/nd500-cc2 < "$out/$bn.ic" > "$out/$bn.s" 2>>"$W/build.log" \
           && $B/nd500-as "$out/$bn.s" -o "$out/$bn.o" 2>>"$W/build.log"; then
            ok=$((ok+1))
        else
            fail=$((fail+1)); echo "FAIL(c) $d/$bn" >> "$W/build.log"
            rm -f "$out/$bn.o"
        fi
        rm -f "$out/$bn.i" "$out/$bn.ic" "$out/$bn.s"
    done

    # Hand-written assembly: the system-call stubs live here, so these matter as
    # much as the C.
    #
    # Run through the PREPROCESSOR first. These files are .s but they are C-
    # preprocessed assembly: sys/sbrk.s has
    #     #define incr  20
    #     #define nbrk  24
    #     w add3 curbrk, b.incr, b.nbrk
    # so "b.incr" is a stack-frame offset, b.20 - not a symbol. Assembling them
    # directly left those names untouched and nd500-as emitted them as UNDEFINED
    # EXTERNALS, which is exactly why linking mknod failed on "incr", "nbrk" and
    # "length" - three symbols that never existed.
    for src in "$L/$d"/*.s; do
        [ -e "$src" ] || continue
        bn=$(basename "$src" .s)
        # -DMAXCAR=32: sys/syscall.s ends with ".set FU1, 20+MAXCAR*4" but
        # includes only <machine/param.h>, which does not define MAXCAR - it is
        # in machine/frame.h, machine/locore.h and h/varargs.h, all three saying
        # 32. Including frame.h here would drag in C declarations that the file
        # deliberately skips (it defines LOCORE first), so the constant is
        # supplied directly. 32 is not a guess: all three headers agree, and the
        # value is the ND-500's CAR register count, fixed by the hardware.
        if $B/nd500-cpp -DMAXCAR=32 $INC "$src" > "$out/$bn.raw" 2>>"$W/build.log"; then
            # Hoist ".set" to the top. sys/syscall.s ends with
            #     .set FU1, 20+MAXCAR*4
            # while using "ents $FU1" thirty lines earlier - a forward reference
            # this assembler cannot resolve. It does not complain: it exits 0
            # and writes NO object file, so the symbol simply goes missing and
            # the failure only shows up as an undefined _syscall at link time.
            # Reordering here keeps the 1988 source untouched.
            awk '/^[ \t]*\.set/ {defs = defs $0 "\n"; next} {body = body $0 "\n"}
                 END {printf "%s%s", defs, body}' \
                "$out/$bn.raw" > "$out/$bn.cs"
            if $B/nd500-as "$out/$bn.cs" -o "$out/$bn.o" 2>>"$W/build.log" \
               && [ -s "$out/$bn.o" ]; then
                ok=$((ok+1))
            else
                fail=$((fail+1)); echo "FAIL(s) $d/$bn" >> "$W/build.log"
                rm -f "$out/$bn.o"
            fi
        else
            fail=$((fail+1)); echo "FAIL(s) $d/$bn" >> "$W/build.log"
        fi
        rm -f "$out/$bn.raw" "$out/$bn.cs"
    done

    printf "  %-16s %3d built  %3d failed\n" "$lib" "$ok" "$fail"
    TOT_OK=$((TOT_OK+ok)); TOT_FAIL=$((TOT_FAIL+fail))
done

echo
echo "total: $TOT_OK objects built, $TOT_FAIL failed"

# ---- csu: the C start-up directory ---------------------------------------
# crt0.s is the program entry and is linked EXPLICITLY, so it goes to libtest
# rather than into the searchable library. mon.s provides mcount, which every
# object compiled by this cc references - leave it out and nothing links.
# gmon.s and x.s define mcount too (the profiling variants); only one may be in
# the search path or the link closes on a duplicate.
echo
echo "csu (start-up and mcount):"
mkdir -p "$ND500X_WORK/libtest" "$W/genlib"
if $B/nd500-cpp $INC "$L/csu/crt0.s" > "$W/crt0.cs" 2>>"$W/build.log" \
   && $B/nd500-as "$W/crt0.cs" -o "$ND500X_WORK/libtest/crt0.o" 2>>"$W/build.log"; then
    echo "  crt0.o   built (linked explicitly, not searched)"
else
    echo "  crt0.o   FAILED"
fi
if $B/nd500-cpp $INC "$L/csu/mon.s" > "$W/mon.cs" 2>>"$W/build.log" \
   && $B/nd500-as "$W/mon.cs" -o "$W/genlib/mon.o" 2>>"$W/build.log"; then
    echo "  mon.o    built (provides mcount)"
else
    echo "  mon.o    FAILED"
fi
rm -f "$W/crt0.cs" "$W/mon.cs"

# ---- symbol index -------------------------------------------------------
# One line per DEFINED symbol: "<symbol> <lib>/<member.o>". linkclose3.sh does
# an exact field match on these, so the member path must be relative to libx.
echo
echo "building symbol index..."
: > "$W/symindex.txt"
for lib in syslib genlib stdiolib netlib hostlib inetlib compat-4.1lib compat-sys5lib; do
    [ -d "$W/$lib" ] || continue
    for o in "$W/$lib"/*.o; do
        [ -e "$o" ] || continue
        m="$lib/$(basename "$o")"
        # A member DEFINES a symbol in two ways, and both must be indexed:
        #   - TEXT/DATA/BSS with EXT      an ordinary definition
        #   - UNDF|EXT with a NONZERO value   a COMMON (tentative definition)
        #
        # Indexing only the first missed errno entirely. It is a common in
        # gen/perror.o - "int errno;" at file scope - so it listed as UNDF and
        # was skipped, even though that object is where it lives. _errno was
        # then the single most requested unresolved symbol across the whole
        # batch (73 times), blocking programs that do nothing more exotic than
        # report a failure.
        $B/nd500-dump -s "$o" 2>/dev/null \
          | awk -v m="$m" '/EXT/ {
                if ($0 !~ /UNDF/)            { print $2, m; next }
                if (strtonum($4) != 0)       { print $2, m }
              }'
    done
done | sort -u > "$W/symindex.txt"
echo "  $(wc -l < "$W/symindex.txt") symbols indexed"
echo
echo "first failures (if any):"
grep -m 8 "^FAIL" "$W/build.log" || echo "  none"
