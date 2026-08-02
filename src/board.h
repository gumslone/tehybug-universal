#pragma once

// The indicator differs between the generic 1MB build (old / first-gen
// TeHyBug, esp-01 based) and the ESP8285 build (TeHyBug universal and Mini).
#if defined(ARDUINO_ESP8266_GENERIC)
#define PIXEL_ACTIVE 0
#define SIGNAL_LED_PIN 1
#endif

// I2C is wired the same way on both boards. The generic build used to declare
// these the other way round (SDA=2, SCL=0), so a first-gen board scanned the
// bus with the two lines crossed and reported "No I2C devices found" however
// many sensors were fitted — every I2C sensor was undetectable there. Keeping
// one definition for both builds is what stops that coming back.
#define I2C_SDA 0
#define I2C_SCL 2

// DHT data line, and the pin that gates the sensor's supply (driven low = on).
//
// Unverified on the generic (esp-01) board: the DHT does not read there and
// swapping these two, which is what the I2C mapping on that board turned out to
// need, did not help either. So the fault is elsewhere - a different pin, or a
// supply that is not gated at all - and the next thing worth knowing is which
// ESP-01 pin the data line is actually soldered to. These are named rather than
// buried in sensors.h so that test is a one-line change.
#define DHT_PIN 2
#define DHT_POWER_PIN 0

// Config-mode button
#define BUTTON_PIN 0

// DS18B20 buses
#define ONE_WIRE_BUS 2
#if !defined(ARDUINO_ESP8266_GENERIC)
#define SECOND_ONE_WIRE_BUS 13
#endif

// Adjust sea level for altitude calculation
#define SEA_LEVEL_PRESSURE_HPA 1026.25
