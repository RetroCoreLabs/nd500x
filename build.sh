#!/usr/bin/env bash
set -euo pipefail
dir="${1:-build}"
mkdir -p "$dir"
cmake -S . -B "$dir"
cmake --build "$dir" -j

