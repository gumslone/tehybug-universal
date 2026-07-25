#pragma once
// WiFi connection management, config portal (WiFiManager) and mDNS.
//
// Expects the following globals (defined in tehybug.ino before this
// header is included): `tehybug`, `wifiManager`, `wifiSsid`,
// `wifiPassword` — plus sleep_modes.h and sensors.h.
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <FS.h>
#include "debug.h"
#include "fw_version.h"

const IPAddress apIP(192, 168, 4, 1);

// Give the radio time to actually associate between attempts, and cap how many
// consecutive failures trigger a reboot.
constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 10000;
constexpr uint8_t WIFI_MAX_ATTEMPTS = 6;

// How long a serving-mode device sleeps before retrying a failed WiFi connect.
// Long enough to ride out a router reboot without draining the battery.
constexpr int WIFI_RETRY_SLEEP_S = 300;

// One reconnect attempt per call, rate-limited. The old version spun in
// `while (!connected) { WiFi.reconnect(); yield(); }` and restarted the device
// on the 10th iteration — all ten fired within microseconds, far quicker than
// an association can complete, so any brief drop rebooted the device instead of
// reconnecting. It also blocked loop() for the whole time.
void connectToWiFi()
{
  static unsigned long lastAttempt = 0;
  static bool attempted = false;
  static uint8_t attempts = 0;

  if (WiFi.status() == WL_CONNECTED) {
    attempts = 0;
    return;
  }

  const unsigned long now = millis();
  // unsigned subtraction, so this stays correct across the millis() rollover
  if (attempted && (now - lastAttempt) < WIFI_RETRY_INTERVAL_MS) {
    return;
  }
  lastAttempt = now;
  attempted = true;

  if (++attempts > WIFI_MAX_ATTEMPTS) {
    D_println(F("WiFi reconnect failed repeatedly, restarting"));
    ESP.restart();
  }
  D_print(F("WiFi reconnect attempt "));
  D_println(attempts);
  WiFi.reconnect();
}

void checkWifi()
{
  if (WiFi.status() != WL_CONNECTED || WiFi.localIP().toString() == "0.0.0.0") {
    connectToWiFi();
  }
}

void configModeCallback(WiFiManager *myWiFiManager) {
  tehybug.device.configMode = true;
  tehybug.pixel.on();
  D_println("Entered wifi config mode");
  D_println(WiFi.softAPIP());
  D_println(myWiFiManager->getConfigPortalSSID());
}

void saveConfigCallback() {
  tehybug.conf.saveConfigCallback();
}

void setupWifi() {
  D_println("Setup WIFI");

  wifiManager.setDebugOutput(true);
  // Set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  // Show every scanned network in the portal. Calling this with no argument
  // defaults to 8 (%), which silently filters weak APs out of the list; -1
  // disables the filter so nothing the radio can see is hidden.
  wifiManager.setMinimumSignalQuality(-1);
  wifiManager.setAPCallback(configModeCallback);
  // Config menu timeout 180 seconds.
  wifiManager.setConfigPortalTimeout(180);
  // Don't bail in <4s: give each association up to 20s and retry a few times.
  // The ESP often drops the first association attempt (reason 2 / AUTH_EXPIRE)
  // and only succeeds on a retry; without this WiFiManager gives up far too
  // fast and falls into the config portal even for a good, saved network.
  wifiManager.setConnectTimeout(20);
  wifiManager.setConnectRetries(3);
  WiFi.hostname(wifiSsid);
  // set custom ip for portal
  wifiManager.setAPStaticIPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  std::vector<const char *> wm_menu = {"wifi", "exit"};
  wifiManager.setShowInfoUpdate(false);
  wifiManager.setShowInfoErase(false);
  wifiManager.setMenu(wm_menu);
  wifiManager.setCustomHeadElement("<style>button {background-color: #1FA67A;}</style>");

  // Only open the blocking AP config portal when config mode is requested
  // (MODE button / first start). In serving mode a failed connect should
  // deep-sleep and retry on wake, not park in the portal draining the battery.
  wifiManager.setEnableConfigPortal(tehybug.device.configMode);

  // Give the WiFi/SDK background tasks a slice and report the heap we are
  // entering the scan/portal with — the scan-results page is built in RAM, so
  // connecting/scanning is most reliable with the heap as free as possible.
  D_print(F("Free heap before WiFi: "));
  D_println(ESP.getFreeHeap());
  yield();

  if (!wifiManager.autoConnect(wifiSsid, wifiPassword)) {
    Serial.println(F("Setup: Wifi failed to connect"));
    yield();

    // Never leave config mode just because the connection failed.
    //
    // This used to clear configMode unconditionally here. When the config
    // portal timed out — the one case where configMode is still true — the
    // device therefore dropped out of config mode *and* had no link: no
    // portal, no station, "Starting live mode" with the IP unset, and every
    // request failing with -1 until the battery ran out, unreachable.
    //
    // In config mode the soft-AP stays up, so leaving the flag alone keeps the
    // device reachable at 192.168.4.1 to fix the credentials.
    if (tehybug.device.configMode) {
      D_println(F("Staying in config mode; reachable on the soft-AP"));
      return;
    }

    // A serving mode cannot do anything without a link, so sleep and retry on
    // the next boot rather than burning power failing every request.
    D_println(F("No WiFi in a serving mode, deep sleep and retry"));
    tehybug.pixel.off();
    startDeepSleep(WIFI_RETRY_SLEEP_S);
    delay(100);
    // Only reached if the board cannot deep-sleep (GPIO16 not wired to RST):
    // fall back to config mode so it stays reachable instead of serving into
    // a dead network.
    D_println(F("Deep sleep unavailable, falling back to config mode"));
    tehybug.device.configMode = true;
    return;
  }
  yield();
  D_println(F("Wifi successfully connected!"));
  tehybug.conf.saveConfig();
}

void setupMdns() {
  // generate module IDs
  String escapedMac = WiFi.macAddress();
  escapedMac.replace(":", "");
  escapedMac.toLowerCase();
  // Set up mDNS responder:
  // "end" must be called before "begin" is called a 2nd time
  // see https://github.com/esp8266/Arduino/issues/7213
  MDNS.end();
  MDNS.begin("tehybug");
  D_println(F("mDNS started"));
  MDNS.addService("http", "tcp", 80);
  MDNS.addServiceTxt("http", "tcp", "mac", escapedMac.c_str());
  MDNS.addServiceTxt("http", "tcp", "device", "TeHyBug");
  MDNS.addServiceTxt("http", "tcp", "version", version);
  MDNS.addServiceTxt("http", "tcp", "endpoint", "/");
}

// Mounts SPIFFS and loads the stored config. Must run before the WiFi
// decision so offline mode (read from config) can skip WiFi entirely.
void mountConfig()
{
  D_println(F("Mounting file system..."));
  if (SPIFFS.begin()) {
    D_println(F("File system successfully mounted."));
    tehybug.conf.loadConfig();
  } else {
    D_println(F("Failed to mount FS"));
  }
}

// Brings up the soft-AP (config mode) / mDNS once WiFi is connected.
void setupNetwork()
{
  if(tehybug.device.configMode)
  {
    WiFi.softAP(wifiSsid, wifiPassword);
  }
  else
  {
    WiFi.softAPdisconnect(true);
  }
  D_println("Setup " + WiFi.gatewayIP().toString());
  D_println("Setup " + WiFi.subnetMask().toString());

  setupMdns();
}

void firstStart()
{
  // test mode for first start
  if(SPIFFS.begin() && !tehybug.conf.configExists())
  {
    if (findI2Csensors() > 0)
    {
      // show green color when sensors are found on first start
      // required for testing the mini board after flashing
      tehybug.pixel.on(0, 255, 0);
      delay(5000);
    }
  }
}
