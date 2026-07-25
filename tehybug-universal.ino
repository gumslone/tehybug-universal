// TeHyBug — WiFi temperature/humidity/air-quality sensor firmware
// for ESP8266/ESP8285 boards.
//
// The sketch is built as a single translation unit: the module headers
// below contain function definitions and are included exactly once, in
// dependency order.
#include "src/debug.h"
#include "src/board.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <FS.h>

// PubSubClient mallocs MQTT_MAX_PACKET_SIZE at construction (boot, before WiFi).
// Its default is left small in the lib so config / offline mode don't waste heap
// on a buffer MQTT never uses; setup() calls mqttClient.setBufferSize() to grow
// it only when MQTT/HA is actually active.
#include <PubSubClient.h>
#include <TickerScheduler.h>
#include <WebSocketsServer.h>

#if !defined(ARDUINO_ESP8266_GENERIC)
#include <WiFiClientSecureBearSSL.h>
#endif
#include <WiFiManager.h>

#include <Wire.h>

#include "src/common_functions.h"
#include "src/fw_version.h"
#include "src/i2cscanner.h"
#include "src/tehybug.h"

/* Global objects */

DHTesp dht;
#if !defined(ARDUINO_ESP8266_GENERIC)
DHTesp dht2;
#endif
TeHyBug tehybug(dht);

char wifiSsid[16];
const char *wifiPassword = "TeHyBug123";

WiFiClient espClient;
#if !defined(ARDUINO_ESP8266_GENERIC)
// https data push; left out of the 1MB mini build to keep OTA possible.
// Lazily created on first HTTPS use (see getClient) so the multi-KB BearSSL
// context isn't allocated at boot — it stays off the heap in config / offline /
// MQTT / http-only operation, leaving more free heap for the WiFi connect.
BearSSL::WiFiClientSecure *espClient_ssl = nullptr;
#endif
HTTPClient httpClient;

PubSubClient mqttClient(espClient);
WiFiManager wifiManager;
ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
ESP8266HTTPUpdateServer httpUpdater;

// 5 data-serving slots (get/post/mqtt/ha/eeprom) + 1 for scenario evaluation
TickerScheduler ticker(6);

/* Modules (function definitions, include order matters) */

#include "src/http_request.h"
#include "src/ha.h"
#include "src/sensors.h"
#include "src/web_api.h"
#include "src/mqtt_service.h"
#include "src/sleep_modes.h"
#include "src/data_service.h"
#include "src/wifi_service.h"

/* Button & LED */

void toggleConfigMode() {
  D_println(F("Config mode changed"));
  tehybug.device.configMode = !tehybug.device.configMode;
  if (tehybug.device.configMode) {
    D_println(F("Config mode activated"));
  } else {
    D_println(F("Config mode deactivated"));
  }
  tehybug.conf.saveConfigCallback();
  tehybug.conf.saveConfig();
  yield();
}

// Drive the signal LED to match config mode: blue while configuring, off in
// any serving / sleep mode. Call this after any change to configMode so the
// LED always reflects the current state.
void updateConfigLed() {
  if (tehybug.device.configMode) {
    tehybug.pixel.on();
  } else {
    tehybug.pixel.off();
  }
}

// How long to wait for a MODE-button press after boot.
//
// The button cannot be held down during reset — GPIO0 low at reset puts the ESP
// into flash mode — so it has to be pressed just *after* the device boots,
// which means the firmware has to wait for it rather than sample the pin once.
//
// Live (non-sleep) modes only boot on a manual reset or power-up, so waiting
// costs nothing there and the window is generous. Sleep and offline modes run
// this on every wake-up, where it is battery time, so theirs stays short.
constexpr unsigned long BUTTON_WINDOW_LIVE_MS = 1000;
constexpr unsigned long BUTTON_WINDOW_SLEEP_MS = 1000;

// How long the MODE button must be held to trigger a factory reset. Long
// enough that it cannot be hit by the short press that toggles config mode.
constexpr unsigned long FACTORY_RESET_HOLD_MS = 20000;

// Idle pause at the end of each live-mode loop. Keeps the loop from spinning
// flat out (which costs power) while staying far below any service interval.
constexpr unsigned long LIVE_LOOP_IDLE_MS = 150;

// Short press toggles config mode, holding for 20 seconds factory-resets.
//
// Without WiFi (offline mode) or with the web server off (live mode), the MODE
// button is the only way back into config mode. The window used to apply to
// offline mode only, leaving live and deep-sleep boots with just the 100 ms
// settle delay — so the press had to land in a fraction of a second.
void checkModeButton() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  delay(100);

  // Already in config mode? Nothing to switch to, so don't delay the boot.
  if (!tehybug.device.configMode && digitalRead(BUTTON_PIN) == HIGH) {
    const bool wakesOften = tehybug.sleepEnabled() || tehybug.device.offlineMode;
    const unsigned long window =
        wakesOften ? BUTTON_WINDOW_SLEEP_MS : BUTTON_WINDOW_LIVE_MS;
    D_print(F("MODE button window (ms): "));
    D_println(window);
    const unsigned long start = millis();
    while (digitalRead(BUTTON_PIN) == HIGH &&
           (millis() - start) < window) {
      delay(10);
    }
  }

  if (digitalRead(BUTTON_PIN) == LOW) {
    const unsigned long pressed = millis();
    bool toggled = false;
    delay(300);
    if (digitalRead(BUTTON_PIN) == LOW) {
      while (digitalRead(BUTTON_PIN) == LOW) {
        if(!toggled)
        {
          toggled = true;
          toggleConfigMode();
          updateConfigLed();
        }
        delay(10);
        if((millis() - pressed) >= FACTORY_RESET_HOLD_MS)
        {
          handleFactoryReset();
        }
      }
    }
  }

  updateConfigLed();
}

/* Periodic data serving (non-sleep mode) */

void addServeTicker(uint8_t slot, int frequencySeconds, std::function<void()> send) {
  const bool added = ticker.add(
    slot, (uint32_t)frequencySeconds * 1000,
  [send](void *) {
    read_sensors();
    yield();
    send();
  },
  nullptr, true);
  // A rejected slot means that service silently never fires again, which is
  // hard to spot in the field — say so instead of failing quietly.
  if (!added) {
    D_print(F("Ticker slot rejected, service will not run: "));
    D_println(slot);
  }
}

void setupServeTickers() {
  uint8_t slot = 0;
  if (tehybug.serveData.get.active) {
    addServeTicker(slot++, tehybug.serveData.get.frequency, httpGet);
  }
  if (tehybug.serveData.post.active) {
    addServeTicker(slot++, tehybug.serveData.post.frequency, httpPost);
  }
  if (tehybug.serveData.mqtt.active) {
    addServeTicker(slot++, tehybug.serveData.mqtt.frequency, mqttSendData);
  }
  if (tehybug.serveData.ha.active) {
    // HA reports on the MQTT interval
    addServeTicker(slot++, tehybug.serveData.mqtt.frequency, haSendData);
  }
  if (tehybug.serveData.eeprom.active) {
    // the EEPROM log is written inside read_sensors(); a bare tick (no
    // network send) is enough to drive it on its own frequency
    addServeTicker(slot++, tehybug.serveData.eeprom.frequency, [] {});
  }
  if (tehybug.anyScenarioActive()) {
    // Scenarios are evaluated in loop() only on the sleep-mode path, so in live
    // mode they never ran. Give them their own tick against fresh readings, on
    // the shortest configured reporting interval. One dedicated ticker (rather
    // than evaluating inside every service tick) keeps a scenario from firing
    // repeatedly when several services are active.
    addServeTicker(slot++, tehybug.minDataFrequency(), serve_scenario);
  }
}

// Probe the RTC + EEPROM module before the offline-mode decision in setup().
// offlineEnabled() depends on peripherals.eeprom, which is otherwise only set
// later inside setupSensors() — too late, so the device would fall through to
// WiFi even with offline mode configured. Must run after checkModeButton(),
// since the MODE button shares GPIO0 with the I2C SDA line.
void detectDataLogModule() {
#if !defined(ARDUINO_ESP8266_GENERIC)
  Wire.begin(I2C_SDA, I2C_SCL);
  i2cScanner::Scanner &scanner = i2cScanner::shared();
  scanner.scan();
  if (scanner.addressExists(0x50)) {
    tehybug.peripherals.eeprom = true;
  }
  if (scanner.addressExists(0x68)) {
    tehybug.peripherals.ds3231 = true;
  }
#endif
}

/* Setup & loop */

void setup() {
  // Serial first: firstStart() scans the I2C bus and logs what it finds, so
  // starting it afterwards threw that output away in debug builds. (There is no
  // wait for the port here — ESP8266's HardwareSerial is always truthy, so the
  // usual `while (!Serial)` loop is a no-op.)
  Serial.begin(115200);
  firstStart();
  snprintf(wifiSsid, sizeof(wifiSsid), "TEHYBUG-%X", ESP.getChipId());

  // load the config before deciding on WiFi: offline mode (stored in the
  // config) must be known before any radio is brought up
  mountConfig();

  // should be called after the fs mount
  tehybug.getDeviceKey();

  // a held MODE button forces config mode (WiFi on) even from offline mode
  checkModeButton();

  // Offline mode is gated on the EEPROM being present, which is otherwise only
  // detected later in setupSensors(). Probe the RTC+EEPROM module now (only
  // when offline mode is configured) so the decision below is correct instead
  // of always falling through to WiFi.
  if (tehybug.device.offlineMode && !tehybug.device.configMode) {
    detectDataLogModule();
  }

  // Offline mode: never bring up WiFi. Just set up the sensors; the loop
  // measures, appends to the EEPROM log and deep-sleeps on the log
  // frequency. A MODE-button press above takes the normal path instead.
  if (tehybug.offlineEnabled() && !tehybug.device.configMode) {
    D_println(F("Starting offline mode"));
    WiFi.mode(WIFI_OFF);
    setupSensors();
    return;
  }

  // Let WiFiManager manage the radio mode: it connects in STA and only brings
  // up AP_STA for the config portal. Forcing WIFI_AP_STA here pins the single
  // radio to the soft-AP channel, so connecting to a router on another channel
  // fails with "no <SSID> found". setupNetwork() brings the AP up afterwards.
  // Sensors, SSL buffers and MQTT are intentionally set up only *after* the
  // WiFi connect/portal below, to keep the heap free for the scan/portal page.
  yield();
  setupWifi();
  D_println(wifiSsid);
  // call after wifi setup
  setupNetwork();

  // The TLS client (espClient_ssl) is set up lazily on first HTTPS push in
  // getClient(), not here, to keep its buffers off the heap until needed.

  // force config when no data serving mode is selected
  if (tehybug.conf.firstStart() || !tehybug.anyServeModeActive()) {
    tehybug.device.configMode = true;
    D_println("Data serving mode not selected or first start");
  }

  if (tehybug.device.configMode) {
    D_println(F("Starting config mode"));
    setupWebServer();
  } else {
    WiFi.softAPdisconnect(true);
    D_println(F("Starting live mode"));
  }

  // setup mqtt / homeassistant
  if (!tehybug.device.configMode && (tehybug.serveData.mqtt.active || tehybug.serveData.ha.active)) {
    updateMqttClient();
    // Longer than the worst-case blocking pass (a DHT read can hold the loop
    // for a few seconds, and an unreachable HTTP target for the request
    // timeout). At the old 10 s the broker dropped the connection whenever a
    // sensor was slow, so MQTT reconnected in a loop instead of publishing.
    mqttClient.setKeepAlive(45);
    mqttClient.setCallback(mqttCallback);
    // Sized to the largest message this firmware actually builds: a ~1 KB HA
    // payload plus its topic and the 5-byte header. It was 4000, permanently
    // mallocing ~2.5 KB more than anything could use out of a ~20 KB heap.
    mqttClient.setBufferSize(1500);
    if (tehybug.serveData.ha.active)
    {
      ha::setupHandle();
    }
    Log(F("Setup"), F("MQTT started"));
  }

  setupSensors();

  // process changes requested by remote control
  if (!tehybug.device.configMode && tehybug.device.remoteControl.active) {
    const String url = tehybug.replacePlaceholders(tehybug.device.remoteControl.url);
    tehybug.handleRemoteControl(http::get(httpClient, getClient(url), url));
  }

  // setup tickers for non-deep-sleep mode
  if (!tehybug.device.configMode && !tehybug.sleepEnabled()) {
    setupServeTickers();
  }

  // Reflect the final config-mode decision on the LED: blue on in config mode,
  // off otherwise. configMode can be turned on above (firstStart / no serving
  // mode) after checkModeButton() already set the LED, and the WiFiManager
  // config-mode callback only fires when the AP portal opens — so set it here
  // unconditionally to cover a normal boot straight into config mode.
  updateConfigLed();
}

void loop() {
  // One decision, resolved in mode_logic.h, instead of re-deriving the mode
  // from boolean combinations here.
  switch (tehybug.mode()) {
    case mode_logic::DeviceMode::Offline:
      // measure, append to the EEPROM log, deep-sleep. No WiFi, no web server
      // and no online scenarios. The deep sleep resets the device, so this
      // restarts setup() on the next wakeup.
      read_sensors();
      yield();
      tehybug.pixel.off();
      startDeepSleep(tehybug.serveData.eeprom.frequency);
      return;

    case mode_logic::DeviceMode::Config:
      MDNS.update();
      server.handleClient();
      yield();
      webSocket.loop();
      break;

    case mode_logic::DeviceMode::DeepSleep:
    case mode_logic::DeviceMode::LightSleep:
      // measure, act, serve, sleep. serve_data() picks deep vs light sleep;
      // after a light sleep execution resumes here on the next iteration with
      // the WiFi association already re-established.
      read_sensors();
      yield();
      serve_scenario();
      yield();
      serve_data();
      break;

    case mode_logic::DeviceMode::Live:
      // served by the tickers below
      break;
  }

  if (tehybug.tickerStop && tehybug.inConfigMode())
  {
    tehybug.tickerStop = false;
    ticker.disableAll();
    updateConfigLed();
  }

  if (tehybug.tickerStart && !tehybug.inConfigMode())
  {
    tehybug.tickerStart = false;
    ticker.enableAll();
    updateConfigLed();
  }
  // update ticker for the non-deep-sleep mode
  ticker.update();

  if (tehybug.inLiveMode()) {
    // reconnect if connection lost
    checkWifi();
    if(tehybug.serveData.mqtt.active || tehybug.serveData.ha.active) {
      // call loop() regularly to allow the library to send MQTT keep alives which
      // avoids being disconnected by the broker
      mqttClient.loop();
    }
    delay(LIVE_LOOP_IDLE_MS); // reduce power consumption
  }
  yield();
  tehybug.finalizeLoop();
}
