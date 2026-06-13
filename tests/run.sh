#!/usr/bin/env bash
# Build and run the native host tests. No board or Arduino toolchain needed —
# the project's hardware-independent logic is compiled against small shims in
# tests/shims/ (a fake I2C EEPROM, a minimal Arduino String).
set -euo pipefail

cd "$(dirname "$0")/.."

CXX="${CXX:-g++}"
OUT=tests/build
mkdir -p "$OUT"
INC="-Itests/shims -Itests -Ilibraries/EepromFS-main"
STD="-std=c++17 -Wall -O1"

echo "==> Building"
"$CXX" $STD $INC tests/test_eeprom.cpp libraries/EepromFS-main/EepromFS.cpp -o "$OUT/test_eeprom"
"$CXX" $STD $INC tests/test_common.cpp -o "$OUT/test_common"
"$CXX" $STD $INC tests/test_i2c.cpp -o "$OUT/test_i2c"

echo "==> Running"
rc=0
"$OUT/test_eeprom" || rc=1
"$OUT/test_common" || rc=1
"$OUT/test_i2c" || rc=1
exit $rc
