#pragma once
// Sensor drivers, detection and reading.
//
// Expects the following globals to exist (defined in tehybug.ino before
// this header is included): `tehybug`, `dht` and — on non-generic
// boards — `dht2`.
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FS.h>
#include "DHTesp.h"
#include "Max44009.h"
#if !defined(ARDUINO_ESP8266_GENERIC)
#include "AHT20.h"
#include "bsec.h"
#endif
#include <AM2320_asukiaaa.h>
#include <ErriezBMX280.h>
#include "debug.h"
#include "board.h"
#include "i2cscanner.h"

// Create BMX280 object I2C address 0x76 or 0x77
ErriezBMX280 bmx280 = ErriezBMX280(0x76);
ErriezBMX280 bmp280 = ErriezBMX280(0x77);

#if !defined(ARDUINO_ESP8266_GENERIC)
Bsec bme680;
uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
uint16_t stateUpdateCounter = 0;
#endif

Max44009 Max44009Lux(0x4A);

#if !defined(ARDUINO_ESP8266_GENERIC)
AHT20 AHT;
#endif

AM2320_asukiaaa am2320;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20_sensors(&oneWire);

#if !defined(ARDUINO_ESP8266_GENERIC)
OneWire secondOneWire(SECOND_ONE_WIRE_BUS);
DallasTemperature second_ds18b20_sensors(&secondOneWire);
#endif

void read_bmx280() {

  if (bmx280.getChipID() == CHIP_ID_BME280) {
    tehybug.addTempHumi("temp", bmx280.readTemperature(), "humi",
                        bmx280.readHumidity());
  } else if (tehybug.sensor.aht20) {
    tehybug.addSensorData("temp2", bmx280.readTemperature());
  } else {
    tehybug.addSensorData("temp", bmx280.readTemperature());
  }
  tehybug.addSensorData("qfe", (bmx280.readPressure() / 100.0F));
  tehybug.addSensorData("alt", bmx280.readAltitude(SEA_LEVEL_PRESSURE_HPA));
}

#if !defined(ARDUINO_ESP8266_GENERIC)
void checkIaqSensorStatus(void) {
  if (bme680.status != BSEC_OK) {
    if (bme680.status < BSEC_OK) {
      D_println("BSEC error code : " + String(bme680.status));
      for (;;)
        delay(1); /* Halt in case of failure */
    } else {
      D_println("BSEC warning code : " + String(bme680.status));
    }
  }

  if (bme680.bme680Status != BME680_OK) {
    if (bme680.bme680Status < BME680_OK) {
      D_println("BME680 error code : " + String(bme680.bme680Status));
      for (;;)
        delay(1); /* Halt in case of failure */
    } else {
      D_println("BME680 warning code : " + String(bme680.bme680Status));
    }
  }
}

void loadBME680State(void) {
  if (SPIFFS.exists("/bsec_state.dat")) {
    File file = SPIFFS.open("/bsec_state.dat", "r");
    if (file) {
      file.read((uint8_t *)bsecState, BSEC_MAX_STATE_BLOB_SIZE);
      file.close();
      bme680.setState(bsecState);
      checkIaqSensorStatus();
      D_println("BME680 state loaded from file");
    }
  }
}

void saveBME680State(void) {
  bool update = false;
  if (stateUpdateCounter == 0) {
    if (bme680.iaqAccuracy >= 3) {
      update = true;
      stateUpdateCounter++;
    }
  } else {
    if ((stateUpdateCounter * 10000) < millis()) {
      update = true;
      stateUpdateCounter++;
    }
  }

  if (update) {
    bme680.getState(bsecState);
    checkIaqSensorStatus();

    File file = SPIFFS.open("/bsec_state.dat", "w");
    if (file) {
      file.write(bsecState, BSEC_MAX_STATE_BLOB_SIZE);
      file.close();
      D_println("BME680 state saved to file");
    }
  }
}

void read_bme680() {

  if (!bme680.run()) { // If no data is available
    checkIaqSensorStatus();
    return;
  }

  D_print(String(bme680.rawTemperature));
  D_print(", " + String(bme680.pressure));
  D_print(", " + String(bme680.rawHumidity));
  D_print(", " + String(bme680.gasResistance));
  D_print(", " + String(bme680.iaq));
  D_print(", " + String(bme680.iaqAccuracy));
  D_print(", " + String(bme680.temperature));
  D_print(", " + String(bme680.humidity));
  D_print(", " + String(bme680.staticIaq));
  D_print(", " + String(bme680.co2Equivalent));
  D_println(", " + String(bme680.breathVocEquivalent));

  tehybug.addSensorData("qfe", (bme680.pressure / 100.0F));
  // Only report CO2 and VOC when accuracy is sufficient
  if (bme680.iaqAccuracy >= 2) {
    tehybug.addSensorData("eco2", bme680.co2Equivalent);
    tehybug.addSensorData("bvoc", bme680.breathVocEquivalent);
  }
  tehybug.addSensorData("iaq", bme680.iaq);
  tehybug.addSensorData("air", (bme680.gasResistance / 1000.0F));
  tehybug.addTempHumi("temp", bme680.temperature, "humi", bme680.humidity);

  // Save state periodically
  saveBME680State();
}
#endif

void read_max44009() {
  const float lux = Max44009Lux.getLux();
  const int err = Max44009Lux.getError();

  if (err != 0) {
    D_print("Error:\t");
    D_println(err);
  } else {
    tehybug.addSensorData("lux", lux);
    D_print("lux:\t");
    D_println(lux);
  }
}

#if !defined(ARDUINO_ESP8266_GENERIC)
void read_aht20() {
  float humidity, temperature;
  if (AHT.getSensor(&humidity, &temperature)) {
    tehybug.addTempHumi("temp", temperature, "humi", (humidity * 100.0F));
  } else {
    Serial.println("GET DATA FROM AHT20 FAIL");
  }
}
#endif

void read_dht_custom(DHTesp &sensor, const String &temp, const String &humi) {
  TempAndHumidity prev = sensor.getTempAndHumidity(); // first read
  if (tehybug.device.configMode)
  {
    tehybug.addTempHumi(temp, prev.temperature, humi, prev.humidity);
    return;
  }
  // keep reading until two consecutive samples agree within 0.5 °C
  for (int i = 0; i < 10; i++) {
    delay(sensor.getMinimumSamplingPeriod());
    TempAndHumidity tehy = sensor.getTempAndHumidity();
    // Check if any reads failed and exit early (to try again).
    if (isnan(tehy.temperature) || isnan(tehy.humidity)) {
      continue;
    }
    if(sensor.getStatusString() == "OK")
    {
      if(isnan(prev.temperature) || isnan(prev.humidity) || fabs(tehy.temperature - prev.temperature) >= 0.5)
      {
        prev.temperature = tehy.temperature;
        prev.humidity = tehy.humidity;
        continue;
      }
      tehybug.addTempHumi(temp, tehy.temperature, humi, tehy.humidity);
      break;
    }
  }
}

void read_dht() {
  pinMode(0, OUTPUT);   // sets the digital pin 0 as output
  digitalWrite(0, LOW); // sets the digital pin 0 on
  read_dht_custom(dht, "temp", "humi");
}

#if !defined(ARDUINO_ESP8266_GENERIC)
void read_second_dht() {
  read_dht_custom(dht2, "temp2", "humi2");
}
#endif

void read_am2320() {
  Wire.begin(I2C_SDA, I2C_SCL);

  for (uint8_t attempt = 0; attempt < 10; attempt++) {
    if (am2320.update() == 0) {
      tehybug.addTempHumi("temp", am2320.temperatureC, "humi", am2320.humidity);
      return;
    }
    yield();
  }
  Serial.println("Error: Cannot update the am2320 sensor values.");
}

void read_ds18b20_custom(DallasTemperature &ds18b20, const String &temp) {
  ds18b20.begin();
  D_print("Requesting temperatures...");
  ds18b20.requestTemperatures(); // Send the command to get temperatures
  D_println("DONE");
  // Only the first sensor on the bus is read.
  const float tempC = ds18b20.getTempCByIndex(0);
  if (tempC != DEVICE_DISCONNECTED_C) {
    D_print("Temperature for the device 1 (index 0) is: ");
    D_println(tempC);
    tehybug.addSensorData(temp, tempC);
  } else {
    Serial.println("Error: Could not read temperature data");
  }
}

void read_ds18b20(void) {
  pinMode(ONE_WIRE_BUS, INPUT_PULLUP);
  delay(100);
  read_ds18b20_custom(ds18b20_sensors, "temp");
}

#if !defined(ARDUINO_ESP8266_GENERIC)
void read_second_ds18b20(void) {
  pinMode(SECOND_ONE_WIRE_BUS, INPUT_PULLUP);
  delay(100);
  read_ds18b20_custom(second_ds18b20_sensors, "temp2");
}

void read_adc() {
  const uint8_t pin = 13;
  pinMode(pin, OUTPUT);
  digitalWrite(pin, HIGH); // on
  delay(100);
  // read the analog in value
  const float sensorValue = analogRead(0);
  tehybug.addSensorData("adc", sensorValue);
  digitalWrite(pin, LOW); // off
}
#endif

void read_sensors() {
  if (tehybug.sensor.bmx) {
    read_bmx280();
  }
#if !defined(ARDUINO_ESP8266_GENERIC)
  if (tehybug.sensor.bme680) {
    read_bme680();
  }
#endif
  if (tehybug.sensor.max44009) {
    read_max44009();
  }
  if (tehybug.sensor.dht) {
    read_dht();
  }
  if (tehybug.sensor.am2320) {
    read_am2320();
  }
  if (tehybug.sensor.ds18b20) {
    read_ds18b20();
  }

#if !defined(ARDUINO_ESP8266_GENERIC)
  if (tehybug.sensor.aht20) {
    read_aht20();
  }
  if (tehybug.sensor.adc) {
    read_adc();
  }
  if (tehybug.sensor.dht_2) {
    read_second_dht();
  }
  if (tehybug.sensor.ds18b20_2) {
    read_second_ds18b20();
  }
#endif
  // offline data log to EEPROM (no-op without RTC + EEPROM module)
  tehybug.logSensorData();
  tehybug.shouldSensorDataBeGarbageCollected(true);
}

uint8_t findI2Csensors() {
  Wire.begin(I2C_SDA, I2C_SCL);
  // required to scan twice to find sensors like am2320
  i2cScanner::Scanner scanner;
  scanner.scan();
  scanner.scan();

  if (scanner.addressExists("0x77")) {
    bmx280 = bmp280;
    tehybug.sensor.bmx = true;
  } else if (scanner.addressExists("0x76")) {
    tehybug.sensor.bmx = true;
  }
  if (scanner.addressExists("0x5c")) {
    tehybug.sensor.am2320 = true;
  }
#if !defined(ARDUINO_ESP8266_GENERIC)
  if (scanner.addressExists("0x77")) {
    tehybug.sensor.bme680 = true;
  }
#endif
  if (scanner.addressExists("0x4a")) {
    tehybug.sensor.max44009 = true;
  }
  if (scanner.addressExists("0x38")) {
    tehybug.sensor.aht20 = true;
  }
#if !defined(ARDUINO_ESP8266_GENERIC)
  if (scanner.addressExists("0x50")) {
    tehybug.peripherals.eeprom = true;
  }
  if (scanner.addressExists("0x68")) {
    tehybug.peripherals.ds3231 = true;
  }
#endif
}

void setupBmx280() {
  // Initialize sensor
  if (!bmx280.begin()) {
    D_println(F("Error: Could not detect sensor"));
    tehybug.sensor.bmx = false;
    return;
  }

  // Print sensor type
  D_print(F("\nSensor type: "));
  switch (bmx280.getChipID()) {
    case CHIP_ID_BMP280:
      D_println(F("BMP280\n"));
      tehybug.sensor.bme680 = false;
      break;
    case CHIP_ID_BME280:
      D_println(F("BME280\n"));
      tehybug.sensor.bme680 = false;
      break;
    default:
      Serial.println(F("Unknown\n"));
      break;
  }

  // In sleep mode the sensor is sampled on demand (forced read),
  // otherwise it samples continuously.
  const BMX280_Mode_e sampling =
      tehybug.sleepEnabled() ? BMX280_MODE_SLEEP : BMX280_MODE_NORMAL;
  bmx280.setSampling(
    sampling,               // SLEEP, FORCED, NORMAL
    BMX280_SAMPLING_X16,    // Temp:  NONE, X1, X2, X4, X8, X16
    BMX280_SAMPLING_X16,    // Press: NONE, X1, X2, X4, X8, X16
    BMX280_SAMPLING_X16,    // Hum:   NONE, X1, X2, X4, X8, X16 (BME280)
    BMX280_FILTER_X16,      // OFF, X2, X4, X8, X16
    BMX280_STANDBY_MS_500); // 0_5, 10, 20, 62_5, 125, 250, 500, 1000
}

#if !defined(ARDUINO_ESP8266_GENERIC)
void setupBme680() {
  D_println(F("BME680 test"));

  bme680.begin(BME680_I2C_ADDR_SECONDARY, Wire);

  D_print("BSEC library version " + String(bme680.version.major) + ".");
  D_print(String(bme680.version.minor) + ".");
  D_print(String(bme680.version.major_bugfix) + ".");
  D_println(String(bme680.version.minor_bugfix));

  checkIaqSensorStatus();

  bsec_virtual_sensor_t sensorList[10] = {
    BSEC_OUTPUT_RAW_TEMPERATURE,
    BSEC_OUTPUT_RAW_PRESSURE,
    BSEC_OUTPUT_RAW_HUMIDITY,
    BSEC_OUTPUT_RAW_GAS,
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_STATIC_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
  };

  // LP mode (3s sampling) calibrates faster but needs more power; ULP
  // mode (300s sampling) is enough for the usual reporting intervals.
  const int minFrequency = tehybug.minDataFrequency();
  if (minFrequency <= 3) {
    bme680.updateSubscription(sensorList, 10, BSEC_SAMPLE_RATE_LP);
    D_println("BME680 sample rate: LP (3s) - Fast calibration, higher power");
  } else {
    bme680.updateSubscription(sensorList, 10, BSEC_SAMPLE_RATE_ULP);
    D_println("BME680 sample rate: ULP (300s) - Balanced calibration/power");
  }
  checkIaqSensorStatus();
  // Load saved calibration state
  loadBME680State();
}
#endif

void setupSensors() {
  if (!tehybug.sensor.dht && !tehybug.sensor.ds18b20) {
    findI2Csensors();
  }
  // bmx280 and bme680 have the same address; setupBmx280() clears the
  // bme680 flag when it identifies a BMP280/BME280 chip
  if (tehybug.sensor.bmx) {
    setupBmx280();
  }
#if !defined(ARDUINO_ESP8266_GENERIC)
  if (tehybug.sensor.bme680) {
    setupBme680();
  }
#endif
  if (tehybug.sensor.max44009) {
    D_print("\nStart max44009_setAutomaticMode : ");
    D_println(MAX44009_LIB_VERSION);

    Max44009Lux.setAutomaticMode();
  }
  if (tehybug.sensor.dht) {
    pinMode(2, INPUT_PULLUP);
    dht.setup(2, DHTesp::DHT22); // Connect DHT sensor to GPIO 2
  }
  else
  {
    dht.setupComfortProfile(); // required for nondht sensors
  }
#if !defined(ARDUINO_ESP8266_GENERIC)
  if (tehybug.sensor.aht20) {
    D_println("AHT20");
    AHT.begin();
  }
  if (tehybug.sensor.dht_2) {
    pinMode(13, INPUT_PULLUP);
    dht2.setup(13, DHTesp::DHT22); // Connect DHT sensor to GPIO 13
  }
#endif
  if (tehybug.peripherals.eeprom) {
    tehybug.eeprom.setup();
    tehybug.syncDataLogMode();  // wipe the log if the period (hourly/monthly) changed
  }
  if (tehybug.peripherals.ds3231) {
    tehybug.time.setup();
  }
  if (tehybug.sensor.am2320) {
    am2320.setWire(&Wire);
  }
  if (tehybug.sensor.ds18b20) {
    pinMode(ONE_WIRE_BUS, INPUT_PULLUP);
  }
}
