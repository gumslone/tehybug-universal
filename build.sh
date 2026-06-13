#!/usr/bin/env bash
set -euo pipefail

# Builds the TeHyBug firmware. By default it uses arduino-cli and places the
# flashable binary in firmware/ (firmware/tehybug.ino.<variant>.bin); set
# TOOL=platformio to build the same variant(s) with PlatformIO instead (output
# stays in PlatformIO's .pio/build/<env>/firmware.bin).
#
# Usage: ./build.sh [esp8285|generic|all] [nodebug|debug]
#   variant  board to build for (default: esp8285)
#   mode     debug enables the sketch's serial debug output (D_print*)
#            and sets the core debug port to Serial; binaries get a
#            _debug suffix (default: nodebug)
#
# Backend (env var):
#   TOOL=arduino     (default) build with arduino-cli
#   TOOL=platformio  build with PlatformIO (alias: pio); override the pio
#                    binary with PIO=/path/to/pio if it is not on PATH
#
# Requires: arduino-cli with the esp8266:esp8266 core (2.7.4); see
# ci/install-deps.sh. All libraries are vendored in ./libraries.

SKETCH_DIR="$(cd "$(dirname "$0")" && pwd)"
TARGET="${1:-esp8285}"
MODE="${2:-nodebug}"
TOOL="${TOOL:-arduino}"

# Matches the Arduino IDE board settings used for the universal board:
# Generic ESP8285 Module, Builtin Led 2, 80 MHz, 26 MHz crystal,
# 2MB (FS:64KB OTA:~992KB), dtr reset, lwIP v2 lower memory, vtables in
# flash, legacy exceptions, erase only sketch, basic SSL.
# (LED_BUILTIN=2 is set for parity with the IDE even though the firmware
# drives SIGNAL_LED_PIN / NeoPixel rather than LED_BUILTIN.)
# The debug port (dbg) option is appended depending on the mode.
ESP8285_OPTS="baud=115200,led=2,xtal=80,CrystalFreq=26,eesz=2M64,ResetMethod=nodemcu,lvl=None____,ip=lm2f,vt=flash,exception=legacy,wipe=none,ssl=basic"

# The sketch only uses HTTPClient's modern begin(client, url) API; the
# legacy 1.1 API would link the unused axTLS stack (~55 KB flash).
CPP_FLAGS="-DHTTPCLIENT_1_1_COMPATIBLE=0"

case "$MODE" in
  debug)
    DBG_OPT="dbg=Serial"
    CPP_FLAGS="$CPP_FLAGS -DDEBUG=1"
    SUFFIX="_debug"
    ;;
  nodebug)
    DBG_OPT="dbg=Disabled"
    # strip WiFiManager's debug strings (~10 KB flash)
    CPP_FLAGS="$CPP_FLAGS -DWM_NODEBUG=1"
    SUFFIX=""
    ;;
  *)
    echo "Usage: $0 [esp8285|generic|all] [nodebug|debug]" >&2
    exit 1
    ;;
esac

# --- PlatformIO backend -----------------------------------------------------
# Maps the variant/mode to PlatformIO environments (esp8285, esp8285_debug,
# generic, generic_debug — see platformio.ini) and builds them with `pio run`.
if [ "$TOOL" = "platformio" ] || [ "$TOOL" = "pio" ]; then
  PIO="${PIO:-pio}"
  command -v "$PIO" >/dev/null 2>&1 || PIO="python3 -m platformio"
  case "$TARGET" in
    esp8285|generic) PIO_ENVS=("$TARGET$SUFFIX") ;;
    all) PIO_ENVS=("esp8285$SUFFIX" "generic$SUFFIX") ;;
    *) echo "Usage: $0 [esp8285|generic|all] [nodebug|debug]" >&2; exit 1 ;;
  esac
  echo "==> Building with PlatformIO: ${PIO_ENVS[*]}"
  pio_args=()
  for e in "${PIO_ENVS[@]}"; do pio_args+=(-e "$e"); done
  $PIO run "${pio_args[@]}"
  for e in "${PIO_ENVS[@]}"; do
    echo "==> Done: .pio/build/$e/firmware.bin"
  done
  exit 0
fi

EXTRA_ARGS=(--build-property "compiler.cpp.extra_flags=$CPP_FLAGS")

ESP8285_FQBN="esp8266:esp8266:esp8285:$ESP8285_OPTS,$DBG_OPT"

# Old / first-generation TeHyBug boards (esp-01 based, generic ESP8266):
# 1MB flash, so the smallest FS (64KB) leaves ~470KB for OTA updates — the
# firmware must stay below that. lwIP without features (no NAPT etc.) saves a
# few KB. (The Mini TeHyBug uses the ESP8285 build, not this one.)
GENERIC_FQBN="esp8266:esp8266:generic:eesz=1M64,ip=lm2n,ssl=basic,$DBG_OPT"

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
  # keep the historical tehybug.ino.* name for the published binaries, in firmware/.
  mkdir -p "$SKETCH_DIR/firmware"
  cp "$build_dir/tehybug-universal.ino.bin" "$SKETCH_DIR/firmware/tehybug.ino.$variant$SUFFIX.bin"
  echo "==> Done: firmware/tehybug.ino.$variant$SUFFIX.bin"
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
