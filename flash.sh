#!/usr/bin/env bash
set -euo pipefail

# Flashes a built TeHyBug firmware binary to an ESP8266/ESP8285 over serial,
# using esptool (the same tool NodeMCU PyFlasher drives). Build first with
# ./build.sh; the binary lands in firmware/tehybug.ino.<variant>.bin.
#
# Usage: ./flash.sh [options]
#   -p PORT    serial port (default: first /dev/cu.usbserial* / *.SLAB* found,
#              or $ESPTOOL_PORT)
#   -f FILE    firmware .bin to flash (default: firmware/tehybug.ino.esp8285.bin)
#   -b BAUD    baud rate: 9600 57600 74880 115200 230400 460800 921600
#              (default: 115200)
#   -m MODE    flash mode: dio (default) | qio | dout
#   -e         erase the whole flash before writing ("yes, wipes all data")
#   -l         list detected serial ports and exit
#   -h         show this help
#
# Examples:
#   ./flash.sh                          # flash esp8285 build to the auto-found port
#   ./flash.sh -e                       # erase all flash, then write
#   ./flash.sh -p /dev/cu.usbserial-110 -b 460800
#   ./flash.sh -f firmware/tehybug.ino.esp8285_debug.bin
#
# Requires: esptool (esptool.py / esptool / `python3 -m esptool`).
#   Install with:  pip3 install esptool

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

PORT="${ESPTOOL_PORT:-}"
FIRMWARE="$SCRIPT_DIR/firmware/tehybug.ino.esp8285.bin"
BAUD=115200
FLASH_MODE=dio
ERASE=0

# Lists likely USB-serial devices on macOS (cu.usbserial*, cu.SLAB*, cu.wchusb*)
# and Linux (ttyUSB*, ttyACM*).
list_ports() {
  ls /dev/cu.usbserial* /dev/cu.SLAB* /dev/cu.wchusb* \
     /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || true
}

while getopts ":p:f:b:m:elh" opt; do
  case "$opt" in
    p) PORT="$OPTARG" ;;
    f) FIRMWARE="$OPTARG" ;;
    b) BAUD="$OPTARG" ;;
    m) FLASH_MODE="$OPTARG" ;;
    e) ERASE=1 ;;
    l) list_ports; exit 0 ;;
    h) sed -n '4,26p' "$0"; exit 0 ;;
    :) echo "Error: -$OPTARG needs an argument" >&2; exit 1 ;;
    \?) echo "Error: unknown option -$OPTARG (try -h)" >&2; exit 1 ;;
  esac
done

case "$FLASH_MODE" in
  dio|qio|dout) ;;
  *) echo "Error: flash mode must be dio, qio or dout" >&2; exit 1 ;;
esac

# Resolve the esptool entry point (mirrors build.sh's pio fallback style).
if command -v esptool.py >/dev/null 2>&1; then
  ESPTOOL=(esptool.py)
elif command -v esptool >/dev/null 2>&1; then
  ESPTOOL=(esptool)
elif python3 -m esptool version >/dev/null 2>&1; then
  ESPTOOL=(python3 -m esptool)
else
  echo "Error: esptool not found. Install it with: pip3 install esptool" >&2
  exit 1
fi

# Auto-detect the port when none was given.
if [ -z "$PORT" ]; then
  PORT="$(list_ports | head -n1 || true)"
  if [ -z "$PORT" ]; then
    echo "Error: no serial port found. Plug in the device, or pass -p PORT." >&2
    echo "Detected ports:"; list_ports | sed 's/^/  /' || echo "  (none)"
    exit 1
  fi
  echo "==> Auto-selected serial port: $PORT"
fi

if [ ! -f "$FIRMWARE" ]; then
  echo "Error: firmware not found: $FIRMWARE" >&2
  echo "Build it first with ./build.sh (creates firmware/tehybug.ino.*.bin)." >&2
  exit 1
fi

echo "==> Flashing $FIRMWARE"
echo "    port=$PORT baud=$BAUD mode=$FLASH_MODE erase=$([ "$ERASE" = 1 ] && echo yes || echo no)"

WRITE_ARGS=(--flash_mode "$FLASH_MODE" --flash_size detect)
[ "$ERASE" = 1 ] && WRITE_ARGS+=(--erase-all)

# --after hard_reset reboots straight into the freshly flashed firmware
# (PyFlasher uses no_reset and asks you to replug; hard_reset is friendlier).
"${ESPTOOL[@]}" --port "$PORT" --baud "$BAUD" --after hard_reset \
  write_flash "${WRITE_ARGS[@]}" 0x0 "$FIRMWARE"

echo "==> Done. The device has been reset into the new firmware."
