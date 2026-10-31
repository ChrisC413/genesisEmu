#!/usr/bin/env bash
# Runs the [rom] tests (label "rom") against the user's own ROM dumps.
#
#   GENESIS_ROM_DIR=/path/to/roms tools/validate-roms.sh [preset]
#
# The preset defaults to mac-release on macOS and win-release elsewhere (use the PowerShell script on
# Windows). Exits non-zero if the build or any ROM test fails, or if GENESIS_ROM_DIR is not set.

set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

if [[ -z "${GENESIS_ROM_DIR:-}" || ! -d "$GENESIS_ROM_DIR" ]]; then
  echo "error: set GENESIS_ROM_DIR to the directory holding your ROM dumps" >&2
  exit 2
fi

if [[ $# -ge 1 ]]; then
  preset="$1"
elif [[ "$(uname -s)" == "Darwin" ]]; then
  preset="mac-release"
else
  preset="win-release"
fi

cmake --preset "$preset"
cmake --build --preset "$preset"
ctest --preset "$preset" -L rom -V
