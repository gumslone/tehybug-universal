#!/usr/bin/env bash
set -euo pipefail

# Builds the TeHyBug firmware with arduino-cli and places the flashable
# binary next to the sketch (tehybug.ino.<variant>.bin).
#
# Usage: ./build.sh [esp8285|generic|all] [nodebug|debug]
#   variant  board to build for (default: esp8285)
#   mode     debug enables the sketch's serial debug output (D_print*)
#            and sets the core debug port to Serial; binaries get a
#            _debug suffix (default: nodebug)
#
# Requires: arduino-cli with the esp8266:esp8266 core (2.7.4); see
# ci/install-deps.sh. All libraries are vendored in ./libraries.

SKETCH_DIR="$(cd "$(dirname "$0")" && pwd)"
TARGET="${1:-esp8285}"
MODE="${2:-nodebug}"

# Matches the Arduino IDE board settings used for the universal board:
# Generic ESP8285 Module, 80 MHz, 26 MHz crystal,
# 2MB (FS:64KB OTA:~992KB), dtr reset, lwIP v2 lower memory, vtables in
# flash, legacy exceptions, erase only sketch, basic SSL.
# The "Builtin Led" menu is omitted: it only defines LED_BUILTIN, which
# this firmware never uses (it drives SIGNAL_LED_PIN / NeoPixel instead).
# The debug port (dbg) option is appended depending on the mode.
ESP8285_OPTS="baud=115200,xtal=80,CrystalFreq=26,eesz=2M64,ResetMethod=nodemcu,lvl=None____,ip=lm2f,vt=flash,exception=legacy,wipe=none,ssl=basic"

case "$MODE" in
  debug)
    DBG_OPT="dbg=Serial"
    EXTRA_ARGS=(--build-property "compiler.cpp.extra_flags=-DDEBUG=1")
    SUFFIX="_debug"
    ;;
  nodebug)
    DBG_OPT="dbg=Disabled"
    EXTRA_ARGS=()
    SUFFIX=""
    ;;
  *)
    echo "Usage: $0 [esp8285|generic|all] [nodebug|debug]" >&2
    exit 1
    ;;
esac

ESP8285_FQBN="esp8266:esp8266:esp8285:$ESP8285_OPTS,$DBG_OPT"

# TeHyBug mini board; basic SSL ciphers to match the esp8285 build
GENERIC_FQBN="esp8266:esp8266:generic:ssl=basic,$DBG_OPT"

build() {
  local variant="$1" fqbn="$2"
  local build_dir="$SKETCH_DIR/.build/$variant$SUFFIX"

  echo "==> Building $variant ($MODE)"
  arduino-cli compile \
    --fqbn "$fqbn" \
    --libraries "$SKETCH_DIR/libraries" \
    --build-path "$build_dir" \
    ${EXTRA_ARGS[@]+"${EXTRA_ARGS[@]}"} \
    "$SKETCH_DIR"

  # arduino-cli names the binary after the sketch (tehybug-universal.ino);
  # keep the historical tehybug.ino.* name for the published binaries.
  cp "$build_dir/tehybug-universal.ino.bin" "$SKETCH_DIR/tehybug.ino.$variant$SUFFIX.bin"
  echo "==> Done: tehybug.ino.$variant$SUFFIX.bin"
}

case "$TARGET" in
  esp8285) build esp8285 "$ESP8285_FQBN" ;;
  generic) build generic "$GENERIC_FQBN" ;;
  all)
    build esp8285 "$ESP8285_FQBN"
    build generic "$GENERIC_FQBN"
    ;;
  *)
    echo "Usage: $0 [esp8285|generic|all] [nodebug|debug]" >&2
    exit 1
    ;;
esac
