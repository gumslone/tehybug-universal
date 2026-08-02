#pragma once
// Declarations of the firmware-wide shared objects, all DEFINED in
// tehybug-universal.ino. Every module includes this instead of assuming the
// sketch declared things before it — which is what used to force the module
// headers to be included in one exact order, documented only by a comment in
// each file ("Expects the following globals..."). With these declarations the
// modules are self-sufficient: each one includes what it uses, and the include
// order in the sketch stops mattering.
//
// The build stays a single translation unit (the sketch includes each module
// header exactly once), so these externs resolve within that one unit; there
// is no separate linking step to get wrong.
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <PubSubClient.h>
#include <WebSocketsServer.h>
#include <WiFiManager.h>
#include <TickerScheduler.h>
#if !defined(ARDUINO_ESP8266_GENERIC)
#include <WiFiClientSecureBearSSL.h>
#endif
#include "tehybug.h"

extern DHTesp dht;
#if !defined(ARDUINO_ESP8266_GENERIC)
extern DHTesp dht2;
#endif
extern TeHyBug tehybug;

// soft-AP SSID ("TEHYBUG-<chipid>") and the portal password
extern char wifiSsid[16];
extern const char *wifiPassword;

extern WiFiClient espClient;
#if !defined(ARDUINO_ESP8266_GENERIC)
// created lazily on first HTTPS use (see getClient in data_service.h)
extern BearSSL::WiFiClientSecure *espClient_ssl;
#endif
extern HTTPClient httpClient;

extern PubSubClient mqttClient;
extern WiFiManager wifiManager;
extern ESP8266WebServer server;
extern WebSocketsServer webSocket;
extern ESP8266HTTPUpdateServer httpUpdater;

// 5 data-serving slots (get/post/mqtt/ha/eeprom) + 1 for scenario evaluation
extern TickerScheduler ticker;
