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
// on a buffer MQTT never uses; setup() calls mqttClient.setBufferSize(4000) to
// grow it only when MQTT/HA is actually active.
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
// https data push; left out of the 1MB mini build to keep OTA possible
BearSSL::WiFiClientSecure espClient_ssl;
#endif
HTTPClient httpClient;

PubSubClient mqttClient(espClient);
WiFiManager wifiManager;
ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
ESP8266HTTPUpdateServer httpUpdater;

TickerScheduler ticker(5);

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

// How long to poll the MODE button on each offline-mode boot. Offline mode
// deep-sleeps right after setup, and the ESP can't tell a manual reset from a
// timer wake (both report a deep-sleep wake), so on every wake we give a short
// window to catch a MODE press rather than sampling the pin once.
constexpr unsigned long BUTTON_WAKE_WINDOW_MS = 1000;

// Short press toggles config mode, holding for 20 seconds factory-resets.
//
// Offline mode brings up no WiFi and deep-sleeps right after setup, so the MODE
// button is the only way back to config mode. On every offline-mode boot the
// button is polled for BUTTON_WAKE_WINDOW_MS so a press shortly after a reset is
// caught. Limited to offline mode, so live / deep-sleep serving boots are
// unaffected.
void checkModeButton() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  delay(100);

  if (tehybug.device.offlineMode && digitalRead(BUTTON_PIN) == HIGH) {
    const unsigned long start = millis();
    while (digitalRead(BUTTON_PIN) == HIGH &&
           (millis() - start) < BUTTON_WAKE_WINDOW_MS) {
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
        if((millis() - pressed) >= 20000)
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
  ticker.add(
    slot, (uint32_t)frequencySeconds * 1000,
  [send](void *) {
    read_sensors();
    yield();
    send();
  },
  nullptr, true);
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
}

// Probe the RTC + EEPROM module before the offline-mode decision in setup().
// offlineEnabled() depends on peripherals.eeprom, which is otherwise only set
// later inside setupSensors() — too late, so the device would fall through to
// WiFi even with offline mode configured. Must run after checkModeButton(),
// since the MODE button shares GPIO0 with the I2C SDA line.
void detectDataLogModule() {
#if !defined(ARDUINO_ESP8266_GENERIC)
  Wire.begin(I2C_SDA, I2C_SCL);
  i2cScanner::Scanner scanner;
  scanner.scan();
  if (scanner.addressExists("0x50")) {
    tehybug.peripherals.eeprom = true;
  }
  if (scanner.addressExists("0x68")) {
    tehybug.peripherals.ds3231 = true;
  }
#endif
}

/* Setup & loop */

void setup() {
  firstStart();
  snprintf(wifiSsid, sizeof(wifiSsid), "TEHYBUG-%X", ESP.getChipId());
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

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

#if !defined(ARDUINO_ESP8266_GENERIC)
  // reduce buffer size and ignore certificate verification
  espClient_ssl.setBufferSizes(256, 256);
  espClient_ssl.setInsecure();
#endif

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
    mqttClient.setKeepAlive(10);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(4000);
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
  // offline mode: measure, append to the EEPROM log, deep-sleep. No WiFi,
  // no web server and no online scenarios. The deep sleep resets the
  // device, so this restarts setup() on the next wakeup.
  if (tehybug.offlineEnabled() && !tehybug.device.configMode) {
    read_sensors();
    yield();
    tehybug.pixel.off();
    startDeepSleep(tehybug.serveData.eeprom.frequency);
    return;
  }
  // config mode
  if (tehybug.device.configMode) {
    MDNS.update();
    server.handleClient();
    yield();
    webSocket.loop();
  }
  // sleep mode: measure, act, serve, sleep
  else if (tehybug.sleepEnabled()) {
    read_sensors();
    yield();
    serve_scenario();
    yield();
    serve_data();
  }

  if (tehybug.tickerStop && tehybug.device.configMode)
  {
    tehybug.tickerStop = false;
    ticker.disableAll();
    updateConfigLed();
  }

  if (tehybug.tickerStart && !tehybug.device.configMode)
  {
    tehybug.tickerStart = false;
    ticker.enableAll();
    updateConfigLed();
  }
  // update ticker for the non-deep-sleep mode
  ticker.update();

  if (!tehybug.device.configMode && !tehybug.sleepEnabled()) {
    // reconnect if connection lost
    checkWifi();
    if(tehybug.serveData.mqtt.active || tehybug.serveData.ha.active) {
      // call loop() regularly to allow the library to send MQTT keep alives which
      // avoids being disconnected by the broker
      mqttClient.loop();
    }
    delay(150); // reduce power consumption
  }
  yield();
  tehybug.finalizeLoop();
}
