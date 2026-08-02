// TeHyBug — WiFi temperature/humidity/air-quality sensor firmware
// for ESP8266/ESP8285 boards.
//
// The sketch is built as a single translation unit: each module header
// contains function definitions and is included exactly once. The modules
// declare the shared objects they use via src/globals.h (defined below), so
// they are self-sufficient and their include order does not matter — the list
// further down is alphabetical to keep it that way.
#include "src/debug.h"
#include "src/board.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#if !defined(ARDUINO_ESP8266_GENERIC)
#include <ESP8266mDNS.h>  // left out of the generic build, see wifi_service.h
#endif
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

/* Modules (alphabetical — order-independent, see the note at the top) */

#include "src/data_service.h"
#include "src/ha.h"
#include "src/http_request.h"
#include "src/mode_button.h"
#include "src/mqtt_service.h"
#include "src/sensors.h"
#include "src/serve_tickers.h"
#include "src/sleep_modes.h"
#include "src/web_api.h"
#include "src/wifi_service.h"

// Idle pause at the end of each live-mode loop. Keeps the loop from spinning
// flat out (which costs power) while staying far below any service interval.
constexpr unsigned long LIVE_LOOP_IDLE_MS = 150;

// Probe the RTC + EEPROM module before the offline-mode decision in setup().
// offlineEnabled() depends on peripherals.eeprom, which is otherwise only set
// later inside setupSensors() — too late, so the device would fall through to
// WiFi even with offline mode configured. Must run after checkModeButton(),
// since the MODE button shares GPIO0 with the I2C SDA line.
void detectDataLogModule() {
#if !defined(ARDUINO_ESP8266_GENERIC)
  // i2cBusBegin probes both port orientations (see sensors.h), so a data-log
  // module attached the mirrored way round is found too.
  i2cBusBegin();
  i2cScanner::Scanner &scanner = i2cScanner::shared();
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

  // Why an offline wake that just entered config mode must reboot first is
  // needsRadioRestart's comment (mode_logic.h): the short version is that the
  // radio was left uninitialised by RF_DISABLED and only a reset brings it up.
  if (mode_logic::needsRadioRestart(
          tehybug.device,
          ESP.getResetInfoPtr()->reason == REASON_DEEP_SLEEP_AWAKE)) {
    D_println(F("Config mode from an offline wake, restarting for the radio"));
    ESP.restart();
  }

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
  if (mode_logic::mustForceConfig(tehybug.conf.firstStart(),
                                  tehybug.serveData)) {
    tehybug.device.configMode = true;
    D_println(F("Data serving mode not selected or first start"));
  }

  // From here on the decisions are the plan's (mode_logic::setupPlan, host-
  // tested); setup() only executes them.
  const mode_logic::SetupPlan plan =
      mode_logic::setupPlan(tehybug.device, tehybug.serveData);

  if (plan.webServer) {
    D_println(F("Starting config mode"));
    setupWebServer();
  } else {
    WiFi.softAPdisconnect(true);
    // Name the mode actually resolved, not "live": a device configured for
    // deep or light sleep announced "Starting live mode" and then went to
    // sleep, which is confusing in exactly the logs used to debug sleep.
    D_print(F("Starting "));
    D_print(tehybug.modeName());
    D_println(F(" mode"));
  }

  // setup mqtt / homeassistant
  if (plan.mqtt) {
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
    if (plan.ha) {
      ha::setupHandle();
    }
    Log(F("Setup"), F("MQTT started"));
  }

  setupSensors();

  // process changes requested by remote control
  if (plan.remoteControl) {
    const String url = tehybug.replacePlaceholders(tehybug.device.remoteControl.url);
    tehybug.handleRemoteControl(http::get(httpClient, getClient(url), url));
  }

  // setup tickers for non-deep-sleep mode
  if (plan.tickers) {
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
      // Offline mode never transmits, so do not power and calibrate the radio
      // on the next wake just to switch it off again in setup(). The MODE
      // button escape is handled by the restart guard in setup().
      startDeepSleep(tehybug.serveData.eeprom.frequency, RF_DISABLED);
      return;

    case mode_logic::DeviceMode::Config:
#if !defined(ARDUINO_ESP8266_GENERIC)
      MDNS.update();
#endif
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
