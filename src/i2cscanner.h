#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <string.h>  // memset
#include "debug.h"

namespace i2cScanner {

class Scanner {
 public:
  void scan() {
    if (scanCount_ >= 2) {
      D_println("I2C scan skipped: max 2 attempts reached");
      return;
    }

    scanCount_++;
    memset(found_, 0, sizeof(found_));
    devicesFound_ = 0;

    D_println("Scanning...");
    for (uint8_t address = 1; address < 127; address++) {
      // The i2c_scanner uses the return value of
      // the Wire.endTransmission to see if
      // a device did acknowledge to the address.
      Wire.beginTransmission(address);
      const uint8_t error = Wire.endTransmission();
      if (error == 0) {
        appendAddress(address);
        devicesFound_++;
        D_print("I2C device found at address ");
        D_println(String(address, HEX));
      }
    }

    // Branches differ only in the log message; identical only because
    // D_println is a no-op when DEBUG is 0.
    // NOLINTNEXTLINE(bugprone-branch-clone)
    if (devicesFound_ == 0)
    {
      D_println("No I2C devices found\n");
    }
    else
    {
      D_println("I2C scan is finished\n");
    }
  }

  // Exact O(1) bit test on the 7-bit address.
  //
  // The results used to be a comma-separated String ("0x38,0x50,...") searched
  // with indexOf, which allocated and grew a String on the heap during boot
  // detection — including the pre-WiFi offline path, where the heap is
  // tightest — and matched on substrings, so a shorter probe like "0x5" would
  // have matched "0x50".
  bool addressExists(uint8_t address) const {
    if (address >= ADDRESS_COUNT) {
      return false;
    }
    return (found_[address >> 3] & (1u << (address & 0x07))) != 0;
  }

  uint8_t devicesFound() const {
    return devicesFound_;
  }

 private:
  void appendAddress(uint8_t address) {
    found_[address >> 3] |= (uint8_t)(1u << (address & 0x07));
  }

  // I2C uses 7-bit addresses, so one bit each fits in 16 bytes
  static constexpr uint8_t ADDRESS_COUNT = 128;

  uint8_t scanCount_{0};
  uint8_t devicesFound_{0};
  uint8_t found_[ADDRESS_COUNT / 8]{};
};

} // namespace i2cScanner
