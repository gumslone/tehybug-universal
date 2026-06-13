#pragma once

// Pin mapping differs between the generic 1MB build (old / first-gen
// TeHyBug, esp-01 based) and the ESP8285 build (TeHyBug universal and Mini).
#if defined(ARDUINO_ESP8266_GENERIC)
#define PIXEL_ACTIVE 0
#define SIGNAL_LED_PIN 1
#define I2C_SDA 2
#define I2C_SCL 0
#else
#define I2C_SDA 0
#define I2C_SCL 2
#endif

// Config-mode button
#define BUTTON_PIN 0

// DS18B20 buses
#define ONE_WIRE_BUS 2
#if !defined(ARDUINO_ESP8266_GENERIC)
#define SECOND_ONE_WIRE_BUS 13
#endif

// Adjust sea level for altitude calculation
#define SEA_LEVEL_PRESSURE_HPA 1026.25
