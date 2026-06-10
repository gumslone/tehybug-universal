#pragma once
// Pushes measurements to the configured targets (HTTP GET/POST, MQTT,
// Home Assistant) and evaluates the automation scenarios.
//
// Expects the following globals (defined in tehybug.ino before this
// header is included): `tehybug`, `espClient`, `espClient_ssl`,
// `httpClient` — plus http_request.h, mqtt_service.h and sleep_modes.h.
#include "debug.h"
#include "common_functions.h"
#include "http_request.h"

WiFiClient & getClient(const String & url)
{
  if (url.startsWith("https")) {
    return espClient_ssl;
  }
  return espClient;
}

void httpGet() {
  const String url = tehybug.replacePlaceholders(tehybug.serveData.get.url);
  http::get(httpClient, getClient(url), url);
}

void httpPost() {
  http::post(httpClient, getClient(tehybug.serveData.post.url),
             tehybug.serveData.post.url,
             tehybug.replacePlaceholders(tehybug.serveData.post.message));
}

void serve_data() {
  if (tehybug.serveData.get.active) {
    httpGet();
    delay(1000);
    if (tehybug.sleepEnabled()) {
      startSleep(tehybug.serveData.get.frequency);
    }
  }

  if (tehybug.serveData.post.active) {
    httpPost();
    delay(1000);
    if (tehybug.sleepEnabled()) {
      startSleep(tehybug.serveData.post.frequency);
    }
  }

  if (tehybug.serveData.mqtt.active) {
    mqttSendData();
    delay(1000);
    if (tehybug.sleepEnabled()) {
      mqttClient.disconnect();
      startSleep(tehybug.serveData.mqtt.frequency);
    }
  }

  // HA reports on the MQTT interval
  if (tehybug.serveData.ha.active) {
    haSendData();
    delay(1000);
    if (tehybug.sleepEnabled()) {
      mqttClient.disconnect();
      startSleep(tehybug.serveData.mqtt.frequency);
    }
  }
}

void checkScenario(Scenario &s) {
  if (!s.active) {
    return;
  }

  float val = 0;
  if (tehybug.sensorData.containsKey(s.data)) {
    val = tehybug.sensorData[s.data];
  }
  const bool conditionMet = (s.condition == "lt" && val < s.value)
                            || (s.condition == "gt" && val > s.value)
                            || (s.condition == "eq" && val == s.value);
  if (!conditionMet) {
    return;
  }

  D_println("condition met");
  D_println(s.url);
  if (s.type == "post") {
    http::post(httpClient, getClient(s.url), s.url,
               tehybug.replacePlaceholders(s.message));
  } else if (isIoScenario(s.type)) {
    const uint8_t pin = ioScenarioPin(s.type);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, ioScenarioLevel(s.type));
  } else {
    http::get(httpClient, getClient(s.url), tehybug.replacePlaceholders(s.url));
  }
}

void serve_scenario() {
  for (Scenario &s : tehybug.scenarios.items) {
    checkScenario(s);
  }
}
