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

void connectToWiFi()
{
  int tryCount = 0;
  while ( WiFi.status() != WL_CONNECTED )
  {
    tryCount++;
    WiFi.reconnect();
    yield();
    if ( tryCount == 10 )
    {
      ESP.restart();
    }
  }
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
    // Serving mode: the 3 connect attempts are done and the portal is disabled,
    // so deep-sleep and retry the connection on the next wake (rides out a
    // temporary AP/router outage). The device reboots into setup() on wake.
    // In config mode the portal was shown instead, so just fall through.
    if (!tehybug.device.configMode) {
      D_println(F("Deep sleep 5 min, will retry WiFi on wake"));
      tehybug.pixel.off();
      startDeepSleep(5 * 60);  // 5 minutes
      delay(100);
    }
    tehybug.device.configMode = false;
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
