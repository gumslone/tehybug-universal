# Tests

Native host tests for the firmware's hardware-independent logic. They compile
and run on a normal desktop compiler — **no board or Arduino toolchain
needed** — by building the real project headers against small shims in
[`shims/`](shims/):

- [`shims/Wire.h`](shims/Wire.h) — a fake 16-bit-addressed I²C EEPROM
  (FT24C256A, 32 KB) backed by an in-memory array, so the real `EepromFS`
  driver runs unchanged.
- [`shims/Arduino.h`](shims/Arduino.h) — a minimal Arduino `String` and a few
  free functions.

## Run

```sh
./tests/run.sh
```

Requires only a C++17 compiler (`g++`/`clang++`). Runs in CI on every push and
pull request (`.github/workflows/tests.yml`).

## Coverage

[`test_eeprom.cpp`](test_eeprom.cpp) exercises the EEPROM data log
([`../eeprom.h`](../eeprom.h)) end-to-end against the fake chip:

- format / mount / capacity (32 slots)
- append + read round-trip; read of a missing file; a full slot keeping only
  what fits
- the **slot-32 date index**: `setFileDate`/`fileDate` round-trips, the
  day-1-vs-day-13 prefix-collision guard, the index slot being hidden from the
  file listing while each day file is tagged with its calendar date
- the **month-rollover** behaviour (a day slot reused in a new month is cleared
  and re-dated) — this is the case the index was added to make correct
- recycling never dropping the index slot, and picking the oldest day file
  correctly across the month wrap-around

[`test_common.cpp`](test_common.cpp) covers the pure helpers in
[`../common_functions.h`](../common_functions.h): `IntFormat` zero-padding,
`GetRSSIasQuality`, `temp2Imp` (°C→°F), the `io13_1`/`io13_0` scenario-type
parsing, and the `key2unit` / `key2name` / `cf2name` lookup tables.

[`test_i2c.cpp`](test_i2c.cpp) covers `i2cScanner` against the fake bus — the
device detection that decides offline mode and which sensors are present.

## Adding tests

`eeprom.h` is unit-testable because it only forward-declares `RtcTime`. Other
headers (`tehybug.h`, etc.) pull in the full Arduino/ESP chain; to test their
logic, either extract the pure functions or extend the shim layer.
