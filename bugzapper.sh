#!/usr/bin/env bash
set -euo pipefail
# Launch BugZapper (the tools/bugzapper submodule) with TeHyBug branding and
# this repo's firmware folder. See https://github.com/gumslone/bugzapper.
# If tools/bugzapper is empty, run: git submodule update --init
DIR="$(cd "$(dirname "$0")" && pwd)"
export BUGZAPPER_TITLE="TeHyBug BugZapper"
export BUGZAPPER_ICON="$DIR/tools/tehybug-icon.png"
export BUGZAPPER_FW_DIR="$DIR/firmware"
exec "$DIR/tools/bugzapper/bugzapper.sh" "$@"
