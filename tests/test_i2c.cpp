// Native host test for the I2C device detection used to decide offline mode
// and which sensors/peripherals are present.
#include "test_framework.h"
#include "../src/i2cscanner.h"

int main() {
  std::printf("Running i2cScanner tests...\n");

  CASE("scan detects only the present addresses");
  // simulate an RTC + EEPROM module plus a BME680
  Wire.wipe();
  Wire.setPresent({0x50, 0x68, 0x77});
  i2cScanner::Scanner scanner;
  scanner.scan();
  CHECK(scanner.addressExists(0x50)); // EEPROM
  CHECK(scanner.addressExists(0x68)); // DS3231 RTC
  CHECK(scanner.addressExists(0x77)); // BME680
  CHECK(!scanner.addressExists(0x4a)); // MAX44009 absent
  CHECK(!scanner.addressExists(0x38)); // AHT20 absent
  CHECK(scanner.devicesFound() == 3);

  CASE("lookup is exact, not a prefix match");
  // Detection used to search a comma-separated String of addresses, where a
  // shorter probe matched a longer address ("0x5" inside "0x50"). Every
  // address is now its own bit, so only an exact match counts.
  CHECK(!scanner.addressExists(0x05));
  CHECK(!scanner.addressExists(0x06)); // 0x68 must not leak into a 0x6x probe
  CHECK(!scanner.addressExists(0x07));
  CHECK(!scanner.addressExists(0x00));

  CASE("addresses outside the 7-bit range are rejected");
  CHECK(!scanner.addressExists(128));
  CHECK(!scanner.addressExists(255));

  CASE("a rescan clears the previous result");
  Wire.wipe();
  Wire.setPresent({0x4a});
  i2cScanner::Scanner second;
  second.scan();
  CHECK(second.addressExists(0x4a));
  CHECK(!second.addressExists(0x50)); // gone with the previous module
  CHECK(second.devicesFound() == 1);

  CASE("boundary addresses land in the right bit");
  // scan() probes 1..126: address 0 and 127 are I2C-reserved, so a device
  // answering there is intentionally not reported.
  Wire.wipe();
  Wire.setPresent({0x01, 0x08, 0x7e, 0x7f});
  i2cScanner::Scanner edges;
  edges.scan();
  CHECK(edges.addressExists(0x01)); // first scanned bit of byte 0
  CHECK(edges.addressExists(0x08)); // first bit of byte 1
  CHECK(edges.addressExists(0x7e)); // last scanned address
  CHECK(!edges.addressExists(0x7f)); // reserved, outside the scan range
  CHECK(!edges.addressExists(0x02));

  return SUMMARY();
}
