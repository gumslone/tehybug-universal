#pragma once
// Sensor drivers, detection and reading.
#include "globals.h"
#include "i2cscanner.h"
#include "board.h"
#include "debug.h"
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FS.h>
#include "DHTesp.h"
#include "Max44009.h"
#include "AHT20.h"
#if !defined(ARDUINO_ESP8266_GENERIC)
#include "bsec.h"
#endif
#include <AM2320_asukiaaa.h>
#include <ErriezBMX280.h>
#if !defined(ARDUINO_ESP8266_GENERIC)
#include "SparkFun_SGP30_Arduino_Library.h"
#endif
#include "debug.h"
#include "board.h"
#include "i2cscanner.h"

// Create BMX280 object I2C address 0x76 or 0x77
ErriezBMX280 bmx280 = ErriezBMX280(0x76);
ErriezBMX280 bmp280 = ErriezBMX280(0x77);

#if !defined(ARDUINO_ESP8266_GENERIC)
Bsec bme680;
uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE] = {0};
#endif

Max44009 Max44009Lux(0x4A);

#if !defined(ARDUINO_ESP8266_GENERIC)
SGP30 sgp30;
#endif

AHT20 AHT;

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
// Checks the BSEC / BME680 driver status. On a hard error the sensor is
// disabled and false is returned — the device keeps running (WiFi, config mode,
// the other sensors) instead of hanging. It used to spin in `for(;;) delay(1)`,
// which feeds the watchdog, so a single sensor fault bricked the device with no
// reset and no way back to config mode.
bool checkIaqSensorStatus(void) {
  bool ok = true;

  if (bme680.status != BSEC_OK) {
    if (bme680.status < BSEC_OK) {
      D_println("BSEC error code : " + String(bme680.status));
      ok = false;
    } else {
      D_println("BSEC warning code : " + String(bme680.status));
    }
  }

  if (bme680.bme680Status != BME680_OK) {
    if (bme680.bme680Status < BME680_OK) {
      D_println("BME680 error code : " + String(bme680.bme680Status));
      ok = false;
    } else {
      D_println("BME680 warning code : " + String(bme680.bme680Status));
    }
  }

  if (!ok) {
    D_println("BME680 disabled after error");
    tehybug.sensor.bme680 = false;
  }
  return ok;
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

// How often the BSEC calibration blob is persisted. Bosch's reference uses
// 6 hours; the state only drifts slowly and every save is a SPIFFS write.
constexpr unsigned long BSEC_STATE_SAVE_PERIOD_MS = 360UL * 60UL * 1000UL;

void saveBME680State(void) {
  static unsigned long lastSaveMs = 0;
  static bool saved = false;

  bool update = false;
  if (!saved) {
    // first save as soon as the sensor reports a fully calibrated reading
    update = (bme680.iaqAccuracy >= 3);
  } else {
    // unsigned subtraction, so this stays correct across the millis() rollover.
    // The old test was `stateUpdateCounter * 10000 < millis()`, whose threshold
    // advanced 10 s per save while millis() advanced in real time — so it
    // rewrote the blob every ~10 seconds, wearing out the flash on a device
    // meant to run for years (and it broke after the 49-day rollover).
    update = (millis() - lastSaveMs) >= BSEC_STATE_SAVE_PERIOD_MS;
  }

  if (update) {
    bme680.getState(bsecState);
    checkIaqSensorStatus();

    File file = SPIFFS.open("/bsec_state.dat", "w");
    if (file) {
      file.write(bsecState, BSEC_MAX_STATE_BLOB_SIZE);
      file.close();
      lastSaveMs = millis();
      saved = true;
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

#if !defined(ARDUINO_ESP8266_GENERIC)
// Relative humidity [%RH] at tempC [°C] to absolute humidity [g/m³], for the
// SGP30's on-chip humidity compensation.
double RHtoAbsolute(float relHumidity, float tempC) {
  const double eSat = 6.11 * pow(10.0, (7.5 * tempC / (237.7 + tempC)));
  const double vaporPressure = (relHumidity * eSat) / 100; // millibars
  // ideal gas law with unit conversions
  return 1000 * vaporPressure * 100 / ((tempC + 273) * 461.5);
}

// The SGP30 takes the humidity as an 8.8 fixed-point number.
uint16_t doubleToFixedPoint(double number) {
  return (uint16_t)floor(number * 256 + 0.5);
}

void read_sgp30() {
  // Feed the last temperature/humidity reading into the SGP30's humidity
  // compensation, when another sensor provided one (values are stored as
  // strings in sensorData, hence the atof).
  const char *tempStr = tehybug.sensorData["temp"].as<const char *>();
  const char *humiStr = tehybug.sensorData["humi"].as<const char *>();
  if (tempStr != nullptr && humiStr != nullptr) {
    const float humidity = atof(humiStr);
    if (humidity > 0) {
      sgp30.setHumidity(doubleToFixedPoint(RHtoAbsolute(humidity, atof(tempStr))));
    }
  }
  const SGP30ERR error = sgp30.measureAirQuality();
  if (error != SGP30_SUCCESS) {
    D_print(F("SGP30 read failed: "));
    D_println((int)error);
    return;
  }
  tehybug.addSensorData("tvoc", (int)sgp30.TVOC);
  tehybug.addSensorData("eco2", (int)sgp30.CO2);
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

void read_aht20() {
  float humidity, temperature;
  if (AHT.getSensor(&humidity, &temperature)) {
    tehybug.addTempHumi("temp", temperature, "humi", (humidity * 100.0F));
  } else {
    D_println(F("GET DATA FROM AHT20 FAIL"));
  }
}

// A DHT needs time after its supply comes up before it answers at all - the
// DHT22 datasheet asks for ~2 s. Probing sooner reads the sensor's own
// power-up as a timeout, which is indistinguishable from "wrong model".
constexpr unsigned long DHT_POWER_UP_MS = 2000;

// Work out which DHT is attached, without the driver's assumption that a
// timeout means DHT11.
//
// DHTesp::AUTO_DETECT tries DHT22 and drops to DHT11 the moment a read times
// out — but a timeout is equally what an absent, unpowered or still-settling
// sensor gives. A DHT22 that was not ready yet is then recorded as a DHT11 and
// talked to with the wrong protocol for the whole session, which is worse than
// the hardcoded DHT22 this replaced. So confirm the fallback: accept DHT11 only
// when a DHT11 read actually answers, and otherwise stay on DHT22 — the more
// common part, and what this firmware assumed before it detected anything.
void setupDht(DHTesp &sensor, uint8_t pin) {
  pinMode(pin, INPUT_PULLUP);
  sensor.setup(pin, DHTesp::DHT22);
  delay(sensor.getMinimumSamplingPeriod());
  if (!isnan(sensor.getTempAndHumidity().temperature)) {
    D_println(F("DHT model detected: DHT22"));
    return;
  }
  sensor.setup(pin, DHTesp::DHT11);
  delay(sensor.getMinimumSamplingPeriod());
  if (!isnan(sensor.getTempAndHumidity().temperature)) {
    D_println(F("DHT model detected: DHT11"));
    return;
  }
  // Neither answered. Stay on DHT22 and let the read path report the failure,
  // rather than locking in a guess that cannot be told apart from a real one.
  sensor.setup(pin, DHTesp::DHT22);
  D_print(F("DHT answered as neither model ("));
  D_print(sensor.getStatusString());
  D_println(F("), assuming DHT22"));
}

void read_dht_custom(DHTesp &sensor, const String &temp, const String &humi) {
  TempAndHumidity prev = sensor.getTempAndHumidity(); // first read
  if (tehybug.device.configMode)
  {
    tehybug.addTempHumi(temp, prev.temperature, humi, prev.humidity);
    return;
  }
  // Keep reading until two consecutive samples agree within 0.5 °C. Capped at
  // DHT_MAX_SAMPLES: each pass waits a full sampling period (2 s on a DHT22),
  // and this runs inside a ticker callback — 10 passes meant up to 20 s with no
  // server.handleClient(), no webSocket.loop() and no mqttClient.loop(), which
  // outlasts the 10 s MQTT keep-alive.
  constexpr int DHT_MAX_SAMPLES = 3;
  bool recorded = false;
  for (int i = 0; i < DHT_MAX_SAMPLES; i++) {
    delay(sensor.getMinimumSamplingPeriod());
    yield();
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
      recorded = true;
      break;
    }
  }
  // Never leave the reading out entirely: if the samples never settled, report
  // the last valid one rather than silently dropping the sensor this cycle.
  if (!recorded && !isnan(prev.temperature) && !isnan(prev.humidity)) {
    tehybug.addTempHumi(temp, prev.temperature, humi, prev.humidity);
  } else if (!recorded) {
    // Every sample was NaN, so nothing at all goes into sensorData and the
    // reading simply vanishes from the payload with no explanation. Say what
    // the driver made of it — "TIMEOUT" points at wiring or the power gate in
    // read_dht(), "CHECKSUM" at a marginal signal.
    D_print(F("DHT read failed for "));
    D_print(temp);
    D_print(F(": "));
    D_println(sensor.getStatusString());
  }
}

// GPIO0 gates the DHT's supply, low being "on". Driving it also takes over the
// I2C line the pin doubles as, which is why a DHT and I2C sensors are
// alternatives on this hardware rather than a combination.
// Returns true when the line was just asserted: the sensor was unpowered
// until now and needs its settle time before it answers. The old Lua
// firmware waited 2 s after grounding on every single read - it could afford
// to, because in its deep-sleep flow every read was a fresh boot. Here the
// state is tracked so live mode pays the wait once, not per read.
bool dhtPowerOn() {
  pinMode(DHT_POWER_PIN, OUTPUT);
  digitalWrite(DHT_POWER_PIN, LOW);
  static bool grounded = false; // per boot, like the pin state itself
  if (grounded) {
    return false;
  }
  grounded = true;
  return true;
}

void read_dht() {
  if (dhtPowerOn()) {
    delay(DHT_POWER_UP_MS); // freshly grounded: give it its power-up time
  }
  read_dht_custom(dht, "temp", "humi");
}

#if !defined(ARDUINO_ESP8266_GENERIC)
void read_second_dht() {
  read_dht_custom(dht2, "temp2", "humi2");
}
#endif

void read_am2320() {
  // Deliberately NO Wire.begin() here: the bus is already up in whichever
  // orientation i2cBusBegin() locked in, and re-beginning with the configured
  // pins flipped it back - so an AM2320 found on the mirrored orientation
  // (the very sensor the mirror probe exists for; its Lua script was the
  // mirrored one) was detected and then could never be read.
  for (uint8_t attempt = 0; attempt < 10; attempt++) {
    if (am2320.update() == 0) {
      tehybug.addTempHumi("temp", am2320.temperatureC, "humi", am2320.humidity);
      return;
    }
    yield();
  }
  D_println(F("Error: Cannot update the am2320 sensor values."));
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
    D_println(F("Error: Could not read temperature data"));
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
  // Average a burst of readings instead of trusting one: the ESP8266 ADC is
  // noisy, and the old Lua firmware averaged 1000 samples for the same
  // reason. 100 at ~100 us each is ~10 ms and settles the value just as well.
  constexpr int ADC_SAMPLES = 100;
  uint32_t sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(0);
  }
  const float sensorValue = (float)sum / ADC_SAMPLES;
  tehybug.addSensorData("adc", sensorValue);
  digitalWrite(pin, LOW); // off
}
#endif

// millis() of the last completed read_sensors(), whoever asked for it (a
// serve ticker, the web API, the display). 0 until the first read. The
// display uses it to top the readings up only when nothing else has, instead
// of adding a second, redundant blocking read to a device that is already
// measuring on a ticker.
unsigned long lastSensorReadMs = 0;

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

  if (tehybug.sensor.aht20) {
    read_aht20();
  }
#if !defined(ARDUINO_ESP8266_GENERIC)
  // after the temp/humi sensors, so the SGP30 gets a fresh humidity value
  // for its compensation
  if (tehybug.sensor.sgp30) {
    read_sgp30();
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
  lastSensorReadMs = millis();
}

// Bring up the I2C bus in whichever line orientation the sensor is actually
// reachable on, and remember the answer for the rest of the session.
//
// The old Lua firmware set the bus up per sensor script, and they did not all
// agree: bme280/bme680/sgp30 used SDA=GPIO0/SCL=GPIO2 while the AM2320 script
// used the mirror image - which worked because the TeHyBug ports are wired as
// mirror images of each other, so whichever way a sensor was attached, one
// script's orientation fit. A single fixed orientation therefore loses
// sensors that the old firmware found. Probe the configured orientation
// first, and only when the bus looks empty try the mirrored one. (Two scans
// per orientation: some sensors, the AM2320 included, only answer after a
// first transaction has woken them.)
void i2cBusBegin() {
  static int8_t mirrored = -1; // -1 undecided, 0 configured, 1 mirrored
  i2cScanner::Scanner &scanner = i2cScanner::shared();
  scanner.resetAttempts();
  if (mirrored == 1) {
    Wire.begin(I2C_SCL, I2C_SDA);
  } else {
    Wire.begin(I2C_SDA, I2C_SCL);
  }
  scanner.scan();
  scanner.scan();
  if (mirrored == -1) {
    if (scanner.devicesFound() > 0) {
      mirrored = 0;
    } else {
      Wire.begin(I2C_SCL, I2C_SDA);
      scanner.resetAttempts();
      scanner.scan();
      scanner.scan();
      if (scanner.devicesFound() > 0) {
        mirrored = 1;
        D_println(F("I2C devices found on the mirrored orientation"));
      }
      // still nothing: leave undecided, so a later call probes both again
    }
  }
}

uint8_t findI2Csensors() {
  i2cScanner::Scanner &scanner = i2cScanner::shared();
  i2cBusBegin();

  // 0x77 is ambiguous: a BME680 and a BMP280/BME280 both answer there, so both
  // flags are set here on purpose and setupBmx280() resolves it by probing —
  // it reads the chip ID and clears bme680 for a known BMx280, or clears bmx
  // when the BMx280 driver fails to start (which means it really is a BME680).
  // The assignment re-points the driver at 0x77 (bmp280 is the same type
  // constructed with that address).
  if (scanner.addressExists(0x77)) {
    bmx280 = bmp280;
    tehybug.sensor.bmx = true;
  } else if (scanner.addressExists(0x76)) {
    tehybug.sensor.bmx = true;
  }
  if (scanner.addressExists(0x5c)) {
    tehybug.sensor.am2320 = true;
  }
#if !defined(ARDUINO_ESP8266_GENERIC)
  if (scanner.addressExists(0x77)) {
    tehybug.sensor.bme680 = true;
  }
#endif
  if (scanner.addressExists(0x4a)) {
    tehybug.sensor.max44009 = true;
  }
  if (scanner.addressExists(0x38)) {
    tehybug.sensor.aht20 = true;
  }
#if !defined(ARDUINO_ESP8266_GENERIC)
  if (scanner.addressExists(0x58)) {
    tehybug.sensor.sgp30 = true;
  }
  if (scanner.addressExists(0x50)) {
    tehybug.peripherals.eeprom = true;
  }
  if (scanner.addressExists(0x68)) {
    tehybug.peripherals.ds3231 = true;
  }
#endif
  // Must return: firstStart() branches on this to show the green "sensors
  // found" LED. Falling off the end is undefined behaviour and made that
  // check read a garbage value.
  return scanner.devicesFound();
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
      D_println(F("Unknown\n"));
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
    // Power the sensor before probing it, not just before each read.
    if (dhtPowerOn()) {
      delay(DHT_POWER_UP_MS);
    }
    setupDht(dht, DHT_PIN);
  }
  else
  {
    dht.setupComfortProfile(); // required for nondht sensors
  }
  if (tehybug.sensor.aht20) {
    D_println("AHT20");
    AHT.begin();
  }
#if !defined(ARDUINO_ESP8266_GENERIC)
  if (tehybug.sensor.sgp30) {
    if (sgp30.begin()) {
      // must run once before measureAirQuality; the first ~15 s of readings
      // are the sensor's warm-up defaults (400 ppm / 0 ppb)
      sgp30.initAirQuality();
    } else {
      // Detected at 0x58 but not answering the init: disable it instead of
      // hanging the boot (the original firmware spun in while(1) here).
      D_println(F("SGP30 detected but init failed, disabled"));
      tehybug.sensor.sgp30 = false;
    }
  }
#endif
#if !defined(ARDUINO_ESP8266_GENERIC)
  if (tehybug.sensor.dht_2) {
    setupDht(dht2, SECOND_ONE_WIRE_BUS); // Port A data pin, shared with 1-Wire
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
