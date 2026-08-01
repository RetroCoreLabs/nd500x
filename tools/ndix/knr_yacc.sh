#!/bin/bash
# Machine-specific roots come from the environment, never from the file.
#   NDIX      NDIX-C checkout (disk images + kernel/)
#   NDIX_B    NDIX-B checkout (authentic 1988 headers/sources)
#   PCC_ND500 pcc-nd500 checkout (the cross toolchain, its bin/)
# The nd500x repo root is derived from this script's own location.
REPO_ROOT=$(cd "$(dirname "$0")/../.." && pwd)
: "${NDIX:?set NDIX to your NDIX-C checkout}"

# knr_yacc.sh <byacc-output.c> - rewrite a modern byacc skeleton into K&R C so
# the 1988 ACE pcc-nd500 front end can compile it.  Only the skeleton is
# touched; the grammar tables and the user's action code are untouched except
# for the blanket "const" removal (the 1988 compiler has no "const").
f=$1
sed -i \
 -e 's/(void)/()/g' \
 -e 's/\bconst\b//g' \
 -e '/^#include <stdlib\.h>/d' \
 -e '/^#include <string\.h>/d' \
 -e 's/^static int yygrowstack(YYSTACKDATA \*data)$/static int yygrowstack(data) YYSTACKDATA *data;/' \
 -e 's/^static void yyfreestack(YYSTACKDATA \*data)$/static yyfreestack(data) YYSTACKDATA *data;/' \
 "$f"
# K\&R declarations for the library routines the skeleton uses.
sed -i '1i char *malloc(); char *realloc(); char *getenv();' "$f"
