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

// How long a just-written MQTT publish and DISCONNECT get to drain out of the
// TCP buffer before the radio sleeps. HTTP needs no equivalent: its response
// has already arrived by the time the sleep path runs.
constexpr unsigned long MQTT_DRAIN_MS = 250;

WiFiClient & getClient(const String & url)
{
#if !defined(ARDUINO_ESP8266_GENERIC)
  if (url.startsWith("https")) {
    // Create the BearSSL client on first use and keep it for the session, so
    // its buffers only cost heap once HTTPS is actually needed.
    if (!espClient_ssl) {
      espClient_ssl = new BearSSL::WiFiClientSecure();
      espClient_ssl->setBufferSizes(256, 256);  // shrink TLS buffers
      espClient_ssl->setInsecure();             // skip cert verification
    }
    return *espClient_ssl;
  }
#endif
  // the generic (1MB) build has no TLS client: https targets fail with a
  // connection error, plain http works
  return espClient;
}

void httpGet() {
  const String url = tehybug.replacePlaceholders(tehybug.serveData.get.url);
  http::get(httpClient, getClient(url), url);
}

void httpPost() {
  // Expand placeholders in the URL too, the same as httpGet does — the POST URL
  // was passed through raw, so a template like %key% in it never resolved.
  const String url = tehybug.replacePlaceholders(tehybug.serveData.post.url);
  http::post(httpClient, getClient(url), url,
             tehybug.replacePlaceholders(tehybug.serveData.post.message));
}

// Pushes the current readings to every configured target, then sleeps once.
//
// Each service used to sleep immediately after its own send. Deep sleep resets
// the device, so with more than one service configured only the first ever ran
// — MQTT never fired if HTTP GET was also enabled. Every block also burned a
// fixed delay(1000), up to 4 s of blocking per pass.
void serve_data() {
  // Nothing can be sent without an association. Skipping is both faster and
  // clearer than watching every request fail with -1 ("connection failed"),
  // which is what a dropped link looked like in the log. The sleep below still
  // runs, so the device rests and tries again on the next wake.
  const bool linked = (WiFi.status() == WL_CONNECTED);
  if (!linked) {
    D_println(F("No WiFi link, skipping this round"));
  }

  // Sleep modes send once and then sleep, so make the broker connection here
  // with a retry budget rather than relying on a next loop iteration.
  if (linked &&
      (tehybug.serveData.mqtt.active || tehybug.serveData.ha.active) &&
      !mqttClient.connected()) {
    mqttEnsureConnected(MQTT_WAKE_CONNECT_BUDGET_MS);
  }

  if (linked && tehybug.serveData.get.active) {
    httpGet();
  }
  if (linked && tehybug.serveData.post.active) {
    httpPost();
  }
  if (linked && tehybug.serveData.mqtt.active) {
    mqttSendData();
  }
  // HA reports on the MQTT interval
  if (linked && tehybug.serveData.ha.active) {
    haSendData();
  }

  if (!tehybug.sleepEnabled()) {
    return;
  }

  // Sleep on the shortest configured interval, so adding a second service can
  // no longer starve the first. The EEPROM log is written on every wake (inside
  // read_sensors), so it only needs to be considered when it is the shortest.
  const int freq = tehybug.wakeIntervalSeconds();
  if (freq <= 0) {
    return;
  }

  if (tehybug.serveData.mqtt.active || tehybug.serveData.ha.active) {
    mqttClient.disconnect();
    // Give the QoS-0 publishes just written and the DISCONNECT packet a moment
    // to actually leave the radio - they sit in the TCP buffer, and sleeping
    // now would drop them. A moment, not the old full second: this is a drain,
    // not a ceremony.
    delay(MQTT_DRAIN_MS);
  }
  // Say what the device will actually do: "Minimum data frequency" above is
  // only the network services' shortest interval, while the EEPROM log can be
  // shorter still and then it decides how often the device wakes.
  D_print(F("Sleeping for (s): "));
  D_print(freq);
  if (tehybug.serveData.eeprom.active &&
      tehybug.serveData.eeprom.frequency == freq) {
    D_print(F("  (set by the EEPROM log interval)"));
  }
  D_println();

  // No settle window for HTTP: the "200" in the log is the response, so by
  // this point the exchange has already made the full round trip and there is
  // nothing left in flight. The old fixed delay(1000) here was measured on
  // hardware as 1.00 s of a 2.45 s wake - 41% of the awake time, spent doing
  // nothing. MQTT is different (QoS-0 bytes still in the TCP buffer) and gets
  // its bounded drain above.
  startSleep(freq);
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
