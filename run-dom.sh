#!/bin/bash
#
# run-dom.sh - run the DOM integration harness against a domain image.
#
# The .dom file and the trace output both live OUTSIDE this repository, so their
# locations come from the environment rather than being baked in:
#
#   ND500X_DOM     path to the .dom file to run          (required)
#   ND500X_STEPS   instruction budget                    (default 100000)
#   ND500X_TRACE   where to write the instruction trace  (default ./nd-trace-500x.txt)
#
# Example:
#   ND500X_DOM=/path/to/nc-a06.dom ./run-dom.sh
#
set -e

# Work from the repo root, wherever this script happens to be checked out.
cd "$(dirname "$0")"

: "${ND500X_DOM:?set ND500X_DOM to the .dom file to run}"

exec ./build/bin/test_dom_integration \
    "$ND500X_DOM" \
    "${ND500X_STEPS:-100000}" \
    --trace-file "${ND500X_TRACE:-./nd-trace-500x.txt}" \
    --radix hex
