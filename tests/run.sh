#!/usr/bin/env bash
# Build and run the native host tests. No board or Arduino toolchain needed —
# the project's hardware-independent logic is compiled against small shims in
# tests/shims/ (a fake I2C EEPROM, a minimal Arduino String).
set -euo pipefail

cd "$(dirname "$0")/.."

CXX="${CXX:-g++}"
OUT=tests/build
mkdir -p "$OUT"

echo "==> Building eeprom tests"
"$CXX" -std=c++17 -Wall -O1 \
  -Itests/shims -Ilibraries/EepromFS-main \
  tests/test_eeprom.cpp libraries/EepromFS-main/EepromFS.cpp \
  -o "$OUT/test_eeprom"

echo "==> Running"
"$OUT/test_eeprom"
