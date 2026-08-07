#!/bin/bash
# Build the missing userland programs, second pass.
#
# Differences from the first pass, each for a measured reason:
#
#   - tftpd is built WITH tftpsubs.c. tftpsubs is not a program - it is tftpd's
#     support file - which is why it "failed" with an undefined _main and why
#     tftpd failed on _read_ahead and _readit. Listing it separately was my
#     mistake, not a build problem.
#
#   - conf and config need y.tab.h, which is generated. config.y ships in the
#     source; the host yacc produces the .c and .h from it, exactly as the
#     original Makefile did. The generated files go in the work tree, never in
#     the preserved source.
#
#   - Programs are built from every .c in their directory when they have one.
set -u
NDIX=/mnt/e/Dev/Ronny/NDIX-C
SP=/home/ronny/repos/nd500x/work
PROG=/mnt/c/Users/ronny/AppData/Local/Temp/claude/E--Dev-Ronny-NDIX-C/8ae946ae-fd1d-44c4-a731-a036a9cf9531/scratchpad/prog.sh
GEN=$SP/gen
mkdir -p "$SP/bin" "$GEN"

# ---- yacc-generated sources for conf/config -------------------------------
for p in conf config; do
    d=$NDIX/baseline/etc/$p
    y=$(ls "$d"/*.y 2>/dev/null | head -1)
    [ -n "$y" ] || continue
    mkdir -p "$GEN/$p"
    # These grammars use the PRE-POSIX yacc action syntax, where an action is
    # introduced with "=":
    #     spec  = { verifysystemspecs(); }
    # Modern yacc/bison dropped that and reports "unexpected =". Strip the "="
    # before an action brace, on a COPY in the work tree - the 1988 grammar is
    # left exactly as it is.
    sed 's/\([[:space:]]\)=[[:space:]]*{/\1{/g' "$y" > "$GEN/$p/$(basename "$y")"
    # BYACC, not the "yacc" on PATH - that is a symlink to bison.yacc, whose
    # output is ANSI C (enum yytokentype, "int yyparse (void)") and cannot be
    # read by a 1988 K&R front end. tools/ndix/knr_yacc.sh exists precisely to
    # rewrite byacc's skeleton into K&R, and expects byacc's shape.
    ( cd "$GEN/$p" && byacc -d "$(basename "$y")" >/dev/null 2>&1 )
    if [ -f "$GEN/$p/y.tab.h" ]; then
        NDIX=$NDIX bash /home/ronny/repos/nd500x/tools/ndix/knr_yacc.sh "$GEN/$p/y.tab.c"
        # The header needs the same treatment: strip prototypes and const.
        sed -i -e 's/(void)/()/g' -e 's/\bconst\b//g' "$GEN/$p/y.tab.h"
        echo "  generated y.tab.[ch] for $p (byacc + K&R rewrite)"
    else
        echo "  byacc FAILED for $p"
    fi
done

build() {   # build <name> <src...>
    n=$1; shift
    if out=$(bash "$PROG" "$n" "$@" 2>&1 | tail -1) && [ -s "$SP/bin/$n" ]; then
        printf "  %-11s OK\n" "$n"; return 0
    fi
    printf "  %-11s %s\n" "$n" "$(echo "$out" | head -1)"; return 1
}

echo
echo "=== multi-file and generated programs ==="
build tftpd "$NDIX/baseline/etc/tftpd.c" "$NDIX/baseline/etc/tftpsubs.c"

for p in conf config; do
    d=$NDIX/baseline/etc/$p
    [ -d "$d" ] || continue
    # param.c is NOT part of the config program - it is a kernel template that
    # config copies into the build directory, which is why it includes
    # "../h/socket.h": paths relative to sys/conf, not to here. Compiling it as
    # program source was my mistake.
    srcs=$(ls "$d"/*.c 2>/dev/null | grep -v '/param\.c$')
    [ -f "$GEN/$p/y.tab.c" ] && srcs="$srcs $GEN/$p/y.tab.c"
    # Two extra search paths: the generated directory for y.tab.h, and the
    # program's OWN directory for config.h - the grammar is compiled from the
    # work tree, so "config.h" no longer resolves beside it.
    CPPFLAGS="-I$GEN/$p -I$d" build "$p" $srcs
done

echo
echo "=== programs that needed the if/ driver headers ==="
for p in etconfig kanalyze pstat panalyze; do
    if [ -f "$NDIX/baseline/etc/$p.c" ]; then
        build "$p" "$NDIX/baseline/etc/$p.c"
    elif [ -d "$NDIX/baseline/etc/$p" ]; then
        build "$p" $(ls "$NDIX/baseline/etc/$p"/*.c)
    fi
done
