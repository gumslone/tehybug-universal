#pragma once
#include <Arduino.h>
#include <Wire.h>
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
    addresses_.clear();
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

  bool addressExists(const char *addr) const {
    return addresses_.indexOf(addr) >= 0;
  }

  uint8_t devicesFound() const {
    return devicesFound_;
  }

 private:
  void appendAddress(uint8_t address) {
    addresses_ += "0x";
    if (address < 16) {
      addresses_ += "0";
    }
    addresses_ += String(address, HEX) + ",";
  }

  uint8_t scanCount_{0};
  uint8_t devicesFound_{0};
  String addresses_;
};

} // namespace i2cScanner
