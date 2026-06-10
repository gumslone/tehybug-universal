#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "debug.h"

namespace i2cScanner {

String addresses;
uint8_t devicesFound{0};

// scan i2c devices
void scan() {
  D_println("Scanning...");
  devicesFound = 0;
  for (uint8_t address = 1; address < 127; address++) {
    // The i2c_scanner uses the return value of
    // the Wire.endTransmission to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    const uint8_t error = Wire.endTransmission();
    if (error == 0) {
      addresses += "0x";
      if (address < 16) {
        addresses += "0";
      }
      addresses += String(address, HEX) + ",";
      devicesFound++;
      D_print("I2C device found at address ");
      D_println(String(address, HEX));
    }
  }
  if (devicesFound == 0)
  {
    D_println("No I2C devices found\n");
  }
  else
  {
    D_println("I2C scan is finished\n");
  }
}

bool addressExists(const char *addr) {
  return addresses.indexOf(addr) >= 0;
}

} // namespace i2cScanner
