// Native host test for the I2C device detection used to decide offline mode
// and which sensors/peripherals are present.
#include "test_framework.h"
#include "../i2cscanner.h"

int main() {
  std::printf("Running i2cScanner tests...\n");
  CASE("scan detects only the present addresses");
  // simulate an RTC + EEPROM module plus a BME680
  Wire.wipe();
  Wire.setPresent({0x50, 0x68, 0x77});
  i2cScanner::scan();
  CHECK(i2cScanner::addressExists("0x50")); // EEPROM
  CHECK(i2cScanner::addressExists("0x68")); // DS3231 RTC
  CHECK(i2cScanner::addressExists("0x77")); // BME680
  CHECK(!i2cScanner::addressExists("0x4a")); // MAX44009 absent
  CHECK(!i2cScanner::addressExists("0x38")); // AHT20 absent
  return SUMMARY();
}
