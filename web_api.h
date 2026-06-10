#pragma once
// Config-mode web interface: HTTP API, websocket push and logging.
//
// Expects the following globals (defined in tehybug.ino before this
// header is included): `tehybug`, `server`, `webSocket`, `wifiManager`,
// `httpUpdater` — plus `read_sensors()` from sensors.h.
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <FS.h>
#include "debug.h"
#include "common_functions.h"
#include "fw_version.h"

constexpr uint8_t MAX_WEBSOCKET_CLIENTS = 10;
String websocketConnection[MAX_WEBSOCKET_CLIENTS];

const char mainPage[] PROGMEM = R"=====(
<!doctype html>
<html>
<head>
<script>
let xhr = new XMLHttpRequest();
xhr.open('GET', '/api/getip');
xhr.send();
xhr.onload = function() {
  if (xhr.status == 200) {
    document.getElementById("ip").innerHTML = xhr.responseText;
  }
};
</script>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1, shrink-to-fit=no">
    <link href="https://tehybug.com/tehybug/v1/css/style.php" rel="stylesheet">
    <script src="https://tehybug.com/tehybug/v1/js/javascript.php"></script>
    <title>TeHyBug</title>
</head>
<body>
<div id="page">
Loading...
<br>If the page doesnt load: make sure that you are connected to your local home network and then open this ip: <span id="ip">tehybug.local</span> with your browser
</div>
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
  sendToWebsocketClients(getInfo(), {"/main", "/firststart", "/api/info"});
}

void sendSensorData() {
  sendToWebsocketClients(getSensor(), {"/main", "/settings"});
}

void sendConfig() {
  sendToWebsocketClients(tehybug.conf.getConfig(),
                         {"/settings", "/setsensor", "/scenarios", "/setsystem"});
}

void Log(String function, String message) {
  D_println(function + ": " + message);
  sendToWebsocketClients("{\"log\":{\"function\":\"" + function +
                         "\",\"message\":\"" + message + "\"}}",
                         {"/main"});
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

void handleGetIp() {
  server.sendHeader("Connection", "close");
  server.send(200, "text/html", WiFi.localIP().toString());
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
                      "\",\"files\":" + tehybug.eeprom.listFilesJson() + "}";
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
