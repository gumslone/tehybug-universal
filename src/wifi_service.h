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

/* WiFi fast reconnect ------------------------------------------------------
 *
 * A full scan + association + DHCP takes a few seconds at ~80 mA, and a
 * deep-sleep device pays it on every wake — which dominates its power budget,
 * since the sleep itself is only ~20 uA. Caching the channel, BSSID and IP
 * from the last successful connection lets the next wake skip both the scan
 * and DHCP, typically well under a second instead of several.
 *
 * The cache lives in RTC memory: it survives deep sleep and is wiped by a power
 * cycle, which is the right lifetime for it. Offsets 0-31 belong to eboot when
 * OTA is in play and 64+ holds the HA discovery memo, so this sits at 40.
 *
 * A hint goes stale whenever the AP changes channel, the router hands out a
 * different subnet, or another access point answers for the same SSID — so a
 * failed fast attempt clears the cache and falls back to the normal scan.
 * Otherwise this would trade battery life for a device that cannot reconnect.
 */
constexpr uint32_t WIFI_HINT_RTC_SLOT = 40;
constexpr uint32_t WIFI_HINT_MAGIC = 0x57494649;  // 'WIFI'
constexpr unsigned long WIFI_HINT_TIMEOUT_MS = 4000;

struct WifiHint {
  uint32_t magic;
  uint32_t crc;
  uint32_t ip;
  uint32_t gw;
  uint32_t mask;
  uint32_t dns;
  uint8_t bssid[6];
  uint8_t channel;
  uint8_t reserved;
};

// RTC user memory is not checksummed by the SDK and holds garbage on a cold
// boot, so the payload is verified before it is trusted.
uint32_t wifiHintCrc(const WifiHint &h) {
  const uint8_t *p = (const uint8_t *)&h.ip;
  const size_t n = sizeof(WifiHint) - offsetof(WifiHint, ip);
  uint32_t v = 2166136261u;
  for (size_t i = 0; i < n; i++) {
    v ^= p[i];
    v *= 16777619u;
  }
  return v;
}

bool loadWifiHint(WifiHint &h) {
  if (!ESP.rtcUserMemoryRead(WIFI_HINT_RTC_SLOT, (uint32_t *)&h, sizeof(h))) {
    return false;
  }
  return h.magic == WIFI_HINT_MAGIC && h.crc == wifiHintCrc(h) &&
         h.channel >= 1 && h.channel <= 14 && h.ip != 0;
}

void clearWifiHint() {
  WifiHint h{};
  ESP.rtcUserMemoryWrite(WIFI_HINT_RTC_SLOT, (uint32_t *)&h, sizeof(h));
}

// Remember what worked, so the next wake can skip the scan and DHCP.
void saveWifiHint() {
  WifiHint h{};
  h.magic = WIFI_HINT_MAGIC;
  h.ip = (uint32_t)WiFi.localIP();
  h.gw = (uint32_t)WiFi.gatewayIP();
  h.mask = (uint32_t)WiFi.subnetMask();
  h.dns = (uint32_t)WiFi.dnsIP();
  h.channel = WiFi.channel();
  const uint8_t *bssid = WiFi.BSSID();
  if (bssid != nullptr) {
    memcpy(h.bssid, bssid, sizeof(h.bssid));
  }
  h.crc = wifiHintCrc(h);
  ESP.rtcUserMemoryWrite(WIFI_HINT_RTC_SLOT, (uint32_t *)&h, sizeof(h));
}

bool tryFastConnect() {
  WifiHint h;
  if (!loadWifiHint(h)) {
    return false; // cold boot, or nothing cached yet
  }
  const String ssid = WiFi.SSID();
  const String psk = WiFi.psk();
  if (ssid.length() == 0) {
    return false; // no stored credentials: the portal has to run
  }

  WiFi.mode(WIFI_STA);

  // Reusing the cached address skips DHCP, but only when the whole set is
  // coherent. A zero or stale DNS entry would otherwise be applied as a static
  // config and every hostname lookup would fail — an MQTT broker given by name
  // then fails to connect (rc=-2) even though the link is up. Skipping the scan
  // is the larger saving anyway, so fall back to DHCP rather than risk that.
  const bool addressUsable =
      h.ip != 0 && h.gw != 0 && h.mask != 0 && h.dns != 0 &&
      ((h.ip & h.mask) == (h.gw & h.mask));
  if (addressUsable) {
    WiFi.config(IPAddress(h.ip), IPAddress(h.gw), IPAddress(h.mask),
                IPAddress(h.dns));
  } else {
    D_println(F("Cached address incomplete, keeping DHCP"));
    WiFi.config(0U, 0U, 0U);
  }

  WiFi.begin(ssid.c_str(), psk.c_str(), h.channel, h.bssid, true);

  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         (millis() - start) < WIFI_HINT_TIMEOUT_MS) {
    delay(10);
  }

  if (WiFi.status() == WL_CONNECTED) {
    D_print(F("WiFi fast reconnect ok, ms: "));
    D_println(millis() - start);
    // Printed because a hostname (an MQTT broker, tehybug.com) is only
    // reachable if the DNS server came through as well.
    D_print(F("  ip "));
    D_print(WiFi.localIP());
    D_print(F("  dns "));
    D_println(WiFi.dnsIP());
    if (WiFi.dnsIP() == IPAddress(0, 0, 0, 0)) {
      D_println(F("  no DNS: dropping the cached address, DHCP on next boot"));
      clearWifiHint();
    }
    return true;
  }

  D_println(F("WiFi fast reconnect failed, falling back to a full scan"));
  clearWifiHint();
  // Note: not WiFi.disconnect(), which erases the stored credentials.
  wifi_station_disconnect();
  WiFi.config(0U, 0U, 0U); // back to DHCP for the normal path
  return false;
}

void setupWifi() {
  D_println("Setup WIFI");

  // Fast path first: this is what makes a deep-sleep wake cheap. It only
  // succeeds when a previous connection cached a still-valid hint.
  if (tryFastConnect()) {
    D_println(F("Wifi successfully connected!"));
    return;
  }

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
  // Cache what the scan just worked out, so the next wake can skip it.
  saveWifiHint();
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
