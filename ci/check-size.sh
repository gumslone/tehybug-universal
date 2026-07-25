#!/usr/bin/env bash
# Guards the firmware size limits that are easy to blow through unnoticed.
#
# Two separate limits apply to an OTA-updatable ESP8266 image:
#
#   1. upload.maximum_size — the flash region the sketch is mapped into.
#   2. spiffs_start / 2    — the ESP8266 updater stages the new image in the
#      free flash between the running sketch and the filesystem, so the sketch
#      can only replace itself over the air while it fits twice below the
#      filesystem.
#
# The ceiling is therefore min(maximum_size, spiffs_start / 2). The two builds
# are limited by different things: the 2MB esp8285 has ample staging room and is
# capped by the ~1MB code-mapping window, while the 1MB generic build (old /
# first-generation boards) is capped by the staging rule and has very little
# room to spare — a few KB of new code can silently make those devices
# un-updatable, which is only discovered when an update fails in the field.
#
# Constants are from the ESP8266 core boards.txt (2.7.4), matching the FQBNs in
# build.sh: generic eesz=1M64, esp8285 eesz=2M64.
#
# Usage: ci/check-size.sh
# Fails (exit 1) if any binary exceeds its ceiling; warns within WARN_PCT.
set -euo pipefail

cd "$(dirname "$0")/.."

# variant|binary|upload.maximum_size|spiffs_start
LIMITS=(
  "generic|firmware/tehybug.ino.generic.bin|958448|962560"
  "esp8285|firmware/tehybug.ino.esp8285.bin|1044464|2031616"
  "esp8285_debug|firmware/tehybug.ino.esp8285_debug.bin|1044464|2031616"
)

WARN_PCT=95   # warn when the image is within 5% of its ceiling
rc=0

for entry in "${LIMITS[@]}"; do
  IFS='|' read -r name bin max_size fs_start <<<"$entry"

  if [ ! -f "$bin" ]; then
    echo "==> $name: $bin not built, skipping"
    continue
  fi

  size=$(wc -c <"$bin" | tr -d ' ')
  ota_ceiling=$((fs_start / 2))
  ceiling=$max_size
  limited_by="flash mapping"
  if [ "$ota_ceiling" -lt "$ceiling" ]; then
    ceiling=$ota_ceiling
    limited_by="OTA staging"
  fi

  pct=$((size * 100 / ceiling))
  headroom=$((ceiling - size))

  if [ "$size" -gt "$ceiling" ]; then
    echo "FAIL $name: $size B exceeds its $ceiling B ceiling ($limited_by) by $((size - ceiling)) B"
    echo "     Devices on this build could no longer be updated over the air."
    rc=1
  elif [ "$pct" -ge "$WARN_PCT" ]; then
    echo "WARN $name: $size B is ${pct}% of the $ceiling B ceiling ($limited_by) — only $headroom B left"
  else
    echo "ok   $name: $size B, ${pct}% of the $ceiling B ceiling ($limited_by), $headroom B left"
  fi
done

exit $rc
