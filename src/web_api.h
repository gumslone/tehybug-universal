#pragma once
// Config-mode web interface: HTTP API, websocket push and logging.
#include "globals.h"
#include "sensors.h"
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <FS.h>
#include "debug.h"
#include "common_functions.h"
#include "fw_version.h"

// Track exactly as many slots as the library will ever hand out
// (WEBSOCKETS_SERVER_CLIENT_MAX, 5). It was 10, so half the String objects
// could never be used and every push scanned them anyway.
constexpr uint8_t MAX_WEBSOCKET_CLIENTS = WEBSOCKETS_SERVER_CLIENT_MAX;
String websocketConnection[MAX_WEBSOCKET_CLIENTS];

const char mainPage[] PROGMEM = R"=====(
<!doctype html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1, shrink-to-fit=no">
    <title>TeHyBug</title>
</head>
<body>
<div id="page">
<h3>TeHyBug</h3>
<b>On your own network this device is at: <span id="ip">tehybug.local</span></b>
<br><br>Loading the full interface...
<br>If it does not load, you are probably still on the TeHyBug access point,
which has no internet. Rejoin your home WiFi and open the address above.
</div>
<script>
// Fill in the device address from the device itself. This is the only part of
// the page that works on the TeHyBug access point, so it must not depend on
// anything loading from the internet.
function setBugIp() {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', '/api/getip');
  xhr.onload = function() {
    var el = document.getElementById("ip");
    if (xhr.status == 200 && el) { el.innerHTML = xhr.responseText; }
  };
  xhr.send();
}
setBugIp();
</script>
<!-- The interface itself is hosted on tehybug.com, so it only loads once the
     browser is back on a network with internet. Both are pulled in without
     blocking: a stylesheet in <head> blocks painting and a plain <script src>
     there blocks parsing, so on the access point (no internet) the browser
     stalled before it drew anything at all and the page came up empty. -->
<link rel="stylesheet" href="https://tehybug.com/tehybug/v1/css/style.php" media="print" onload="this.media='all'">
<script src="https://tehybug.com/tehybug/v1/js/javascript.php" defer></script>
</body>
</html>
)=====";

/* Device info / sensor JSON */

String getInfo() {
  DynamicJsonDocument root(1024);
  root["gumboardVersion"] = version;
  root["sketchSize"] = ESP.getSketchSize();
  root["freeSketchSpace"] = ESP.getFreeSketchSpace();
  root["wifiRSSI"] = String(WiFi.RSSI());
  root["wifiQuality"] = GetRSSIasQuality(WiFi.RSSI());
  root["wifiSSID"] = WiFi.SSID();
  root["ipAddress"] = WiFi.localIP().toString();
  root["freeHeap"] = ESP.getFreeHeap();
  root["chipID"] = ESP.getChipId();
  root["cpuFreqMHz"] = ESP.getCpuFreqMHz();
  root["sleepModeActive"] = tehybug.sleepEnabled();
  root["deepSleepMax"] = (int)(ESP.deepSleepMax() / 1000000);
  root["key"] = tehybug.device.key;
  String json;
  serializeJson(root, json);
  return json;
}

String getSensor() {
  read_sensors();
  String json;
  serializeJson(tehybug.sensorData, json);
  return json;
}

/* Websocket push */

// sends the message to every websocket client connected on one of the urls
void sendToWebsocketClients(const String &message, std::initializer_list<const char *> urls) {
  if (webSocket.connectedClients() == 0) {
    return;
  }
  for (uint8_t i = 0; i < MAX_WEBSOCKET_CLIENTS; i++) {
    for (const char *url : urls) {
      if (websocketConnection[i] == url) {
        webSocket.sendTXT(i, message.c_str());
        break;
      }
    }
  }
}

void sendDeviceInfo() {
  // "/settings" included because the Cloud and Home Assistant settings pages
  // connect under that websocket url (see connectionStart in gumboard.js) and
  // display the device key. Without it those pages never received the info
  // message and their key field stayed on "Loading...".
  sendToWebsocketClients(getInfo(),
                         {"/main", "/firststart", "/api/info", "/settings"});
}

void sendSensorData() {
  // "/datalog" included so its template field can offer "fill from my
  // sensors" - the page needs to know which readings this device produces.
  sendToWebsocketClients(getSensor(), {"/main", "/settings", "/datalog"});
}

void sendConfig() {
  sendToWebsocketClients(tehybug.conf.getConfig(),
                         {"/settings", "/setsensor", "/scenarios", "/setsystem", "/datalog"});
}

// Sends a log line to the dashboard websocket.
//
// Takes its arguments by reference (it used to copy both Strings on every
// call, and it is called on every MQTT publish and retry), and builds the JSON
// with ArduinoJson so a quote or backslash in a topic or payload can no longer
// produce malformed JSON on the wire. The payload is only built when a client
// is actually connected.
void Log(const String &function, const String &message) {
  D_println(function + ": " + message);
  if (webSocket.connectedClients() == 0) {
    return;
  }
  DynamicJsonDocument doc(512);
  const JsonObject log = doc.createNestedObject("log");
  log["function"] = function;
  log["message"] = message;
  String out;
  serializeJson(doc, out);
  sendToWebsocketClients(out, {"/main"});
}

/* HTTP API */

void handleGetMainPage() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/html", mainPage);
}

void handleNotFound() {
  if (server.method() == HTTP_OPTIONS) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(204);
    return;
  }
  server.sendHeader("Location", "/update", true);
  server.send(302, "text/plain", "");
}

void handleSetConfig() {
  DynamicJsonDocument json(3072);
  const auto error = deserializeJson(json, server.arg("plain"));
  server.sendHeader("Connection", "close");
  if (!error) {
    Log(("SetConfig"), ("Incoming Json length: " + String(measureJson(json))));
    // extract the data
    JsonObject object = json.as<JsonObject>();
    tehybug.conf.setConfig(object);
    server.send(200, "application/json", "{\"response\":\"OK\"}");
  } else {
    server.send(406, "application/json", "{\"response\":\"Not Acceptable\"}");
  }
}

void handleGetConfig() {
  server.sendHeader("Connection", "close");
  server.send(200, "application/json", tehybug.conf.getConfig());
}

void handleGetInfo() {
  server.sendHeader("Connection", "close");
  server.send(200, "application/json", getInfo());
}

void handleGetSensor() {
  server.sendHeader("Connection", "close");
  server.send(200, "application/json", getSensor());
}

// The address to reach the device on once you are back on your own network.
// Served plain so the config-mode page can show it while you are still on the
// device's own AP, where there is no internet and nothing else loads.
void handleGetIp() {
  server.sendHeader("Connection", "close");
  const IPAddress ip = WiFi.localIP();
  // 0.0.0.0 means the device has not joined a network yet — printing that as an
  // address to browse to just sends people to a dead end.
  server.send(200, "text/html",
              ip == IPAddress(0, 0, 0, 0)
                  ? String(F("not on your network yet - set WiFi first"))
                  : ip.toString());
}

// GET /api/datalog            -> {"active":...,"time":"...","files":[...]}
// GET /api/datalog?file=10.txt -> file content as text
void handleGetDataLog() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Connection", "close");

  if (!tehybug.dataLogAvailable() || !tehybug.eeprom.mounted()) {
    server.send(200, "application/json", "{\"active\":false}");
    return;
  }

  if (server.hasArg("file")) {
    server.send(200, "text/plain", tehybug.eeprom.read(server.arg("file").c_str()));
    return;
  }

  tehybug.time.update();
  const String json = "{\"active\":true,\"timeSet\":" +
                      String(tehybug.time.isTimeSet() ? "true" : "false") +
                      ",\"time\":\"" + tehybug.time.timestamp() +
                      // detected chip capacity and the resulting day-file size,
                      // so the Data Log page can show which EEPROM was found
                      // instead of the user having to read the serial log
                      "\",\"capacity\":" + String(tehybug.eeprom.capacityBytes()) +
                      ",\"slotBytes\":" + String(tehybug.eeprom.slotBytes()) +
                      ",\"files\":" + tehybug.eeprom.listFilesJson() + "}";
  server.send(200, "application/json", json);
}

// GET /api/settime?y=2026&mo=6&d=10&wd=4&h=18&mi=45&s=30
// sets the DS3231 clock (usually from the browser's local time)
void handleSetTime() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Connection", "close");

  if (!tehybug.peripherals.ds3231) {
    server.send(409, "application/json", "{\"response\":\"No RTC detected\"}");
    return;
  }
  if (!server.hasArg("y") || !server.hasArg("mo") || !server.hasArg("d") ||
      !server.hasArg("h") || !server.hasArg("mi")) {
    server.send(400, "application/json", "{\"response\":\"Missing parameter\"}");
    return;
  }

  tehybug.time.setTime(server.arg("y").toInt(), server.arg("mo").toInt(),
                       server.arg("d").toInt(), server.arg("wd").toInt(),
                       server.arg("h").toInt(), server.arg("mi").toInt(),
                       server.arg("s").toInt());
  server.send(200, "application/json",
              "{\"response\":\"OK\",\"time\":\"" + tehybug.time.timestamp() + "\"}");
}

void handleFactoryReset() {
  tehybug.pixel.on(255, 0, 0);
  D_println("Factory reset!");
#if !defined(ARDUINO_ESP8266_GENERIC)
  // Wipe the logged data on the external EEPROM too. Factory reset is triggered
  // by a 20s MODE-button hold, and the button shares GPIO0 with the I2C SDA
  // line — so wait for it to be released (with a timeout) before driving I2C,
  // then detect and format the EEPROM (setupSensors() hasn't mounted it yet on
  // this path).
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  const unsigned long releaseStart = millis();
  while (digitalRead(BUTTON_PIN) == LOW && (millis() - releaseStart) < 5000) {
    delay(10);
  }
  Wire.begin(I2C_SDA, I2C_SCL);
  i2cScanner::Scanner &scanner = i2cScanner::shared();
  scanner.scan();
  if (scanner.addressExists(0x50)) {
    tehybug.peripherals.eeprom = true;
    tehybug.eeprom.format();
  }
#endif
  SPIFFS.format();
  wifiManager.resetSettings();
  yield();
  ESP.restart();
}

/* Websocket events */

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload,
                    size_t length) {
  switch (type) {
    case WStype_DISCONNECTED: {
        Log("WebSocketEvent", "[" + String(num) + "] Disconnected!");
        websocketConnection[num] = "";
        break;
      }
    case WStype_CONNECTED: {
        websocketConnection[num] = String((char *)payload);
        const IPAddress ip = webSocket.remoteIP(num);
        Log("WebSocketEvent", "[" + String(num) + "] Connected from " +
            ip.toString() +
            " url: " + websocketConnection[num]);
        // send message to client
        sendDeviceInfo();
        sendSensorData();
        sendConfig();
        break;
      }
    case WStype_TEXT: {
        if (((char *)payload)[0] == '{') {
          DynamicJsonDocument json(1024);
          deserializeJson(json, payload);
          Log("WebSocketEvent",
              "Incoming Json length: " + String(measureJson(json)));
          if (websocketConnection[num] == "/setConfig") {
            // extract the data
            JsonObject object = json.as<JsonObject>();
            tehybug.conf.setConfig(object);
          }
        }
        break;
      }
    default:
      break;
  }
}

void setupWebServer() {
  httpUpdater.setup(&server);
  server.on(F("/api/info"), HTTP_GET, handleGetInfo);
  server.on(F("/api/config"), HTTP_POST, handleSetConfig);
  server.on(F("/api/config"), HTTP_GET, handleGetConfig);
  server.on(F("/api/sensor"), HTTP_GET, handleGetSensor);
  server.on(F("/api/datalog"), HTTP_GET, handleGetDataLog);
  server.on(F("/api/settime"), HTTP_GET, handleSetTime);
  server.on(F("/api/getip"), HTTP_GET, handleGetIp);
  server.on(F("/"), HTTP_GET, handleGetMainPage);
  server.onNotFound(handleNotFound);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Log(F("Setup"), F("Webserver started"));
}
