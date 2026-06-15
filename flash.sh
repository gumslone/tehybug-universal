#!/usr/bin/env bash
set -euo pipefail
# Run BugZapper's CLI flasher (the tools/bugzapper submodule) against this repo's
# firmware/ folder. See https://github.com/gumslone/bugzapper for the options.
# If tools/bugzapper is empty, run: git submodule update --init
DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"  # so the default ./firmware/*.bin resolves to this repo
exec "$DIR/tools/bugzapper/flash.sh" "$@"
