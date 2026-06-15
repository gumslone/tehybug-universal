#!/usr/bin/env bash
set -euo pipefail
# Launch BugZapper (the tools/bugzapper submodule) for this repo's firmware.
# Uses BugZapper's own icon; just sets the title and firmware folder.
# See https://github.com/gumslone/bugzapper.
# If tools/bugzapper is empty, run: git submodule update --init
DIR="$(cd "$(dirname "$0")" && pwd)"
export BUGZAPPER_TITLE="TeHyBug BugZapper"
export BUGZAPPER_FW_DIR="$DIR/firmware"
exec "$DIR/tools/bugzapper/bugzapper.sh" "$@"
