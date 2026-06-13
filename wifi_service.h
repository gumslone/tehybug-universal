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
  wifiManager.setMinimumSignalQuality();
  wifiManager.setAPCallback(configModeCallback);
  // Config menu timeout 180 seconds.
  wifiManager.setConfigPortalTimeout(180);
  WiFi.hostname(wifiSsid);
  // set custom ip for portal
  wifiManager.setAPStaticIPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));

  std::vector<const char *> wm_menu = {"wifi", "exit"};
  wifiManager.setShowInfoUpdate(false);
  wifiManager.setShowInfoErase(false);
  wifiManager.setMenu(wm_menu);
  wifiManager.setCustomHeadElement("<style>button {background-color: #1FA67A;}</style>");
  if (!wifiManager.autoConnect(wifiSsid, wifiPassword)) {
    Serial.println(F("Setup: Wifi failed to connect and hit timeout"));
    tehybug.device.configMode = false;
    delay(3000);
    // Sleep and retry on the next wakeup
    startSleep(9000);
    delay(5000);
  }
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
    findI2Csensors();
    if(i2cScanner::devicesFound > 0)
    {
      // show green color when sensors are found on first start
      // required for testing the mini board after flashing
      tehybug.pixel.on(0, 255, 0);
      delay(5000);
    }
  }
}
