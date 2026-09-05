#pragma once
// Pushes measurements to the configured targets (HTTP GET/POST, MQTT,
// Home Assistant) and evaluates the automation scenarios.
#include "globals.h"
#include "mqtt_service.h"
#include "sleep_modes.h"
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
      // 512 is the smallest the core accepts - it silently clamps anything
      // lower (this call used to ask for 256 and claim it worked). The small
      // receive buffer only holds if the server negotiates MFLN; against one
      // that does not, the handshake fails and the request reports -1. The
      // default cloud endpoint is plain http, so this only affects
      // user-configured https targets.
      espClient_ssl->setBufferSizes(512, 512);
      // Optional certificate check: with a SHA-1 fingerprint configured the
      // server's certificate must match it exactly (no clock needed, unlike a
      // CA chain). Without one the connection is encrypted but unverified.
      const String &fp = tehybug.serveData.httpsFingerprint;
      if (fp.length() == 0) {
        espClient_ssl->setInsecure();
      } else if (!espClient_ssl->setFingerprint(fp.c_str())) {
        // Fail closed: a pin that cannot be parsed must not silently turn
        // into "no check". With nothing trusted, BearSSL refuses to connect.
        D_println(F("HTTPS fingerprint is malformed - https requests will fail"));
      }
    }
    return *espClient_ssl;
  }
#endif
  // the generic (1MB) build has no TLS client: https targets fail with a
  // connection error, plain http works
  return espClient;
}

// After a failed https request, what BearSSL objected to. A certificate
// that does not match the configured fingerprint only reaches the HTTP layer
// as "connection lost"; this names the reason on the dashboard log.
void Log(const String &function, const String &message); // web_api.h, later in the sketch
void logSslError() {
#if !defined(ARDUINO_ESP8266_GENERIC)
  if (!espClient_ssl) {
    return;
  }
  char reason[96];
  const int code = espClient_ssl->getLastSSLError(reason, sizeof(reason));
  if (code != 0) {
    Log(F("HTTPS"), String(F("TLS error ")) + String(code) + ": " + reason);
  }
#endif
}

void httpGet() {
  const String url = tehybug.replacePlaceholders(tehybug.serveData.get.url);
  http::get(httpClient, getClient(url), url);
  logSslError();
}

void httpPost() {
  // Expand placeholders in the URL too, the same as httpGet does — the POST URL
  // was passed through raw, so a template like %key% in it never resolved.
  const String url = tehybug.replacePlaceholders(tehybug.serveData.post.url);
  http::post(httpClient, getClient(url), url,
             tehybug.replacePlaceholders(tehybug.serveData.post.message));
  logSslError();
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

  // The decisions live in mode_logic::servePlan(), host-tested; this function
  // only executes them against the hardware.
  const mode_logic::ServePlan plan =
      mode_logic::servePlan(tehybug.serveData, tehybug.device, linked);

  // Sleep modes send once and then sleep, so make the broker connection here
  // with a retry budget rather than relying on a next loop iteration.
  if (plan.connectMqtt && !mqttClient.connected()) {
    mqttEnsureConnected(MQTT_WAKE_CONNECT_BUDGET_MS);
  }

  if (plan.sendGet) {
    httpGet();
  }
  if (plan.sendPost) {
    httpPost();
  }
  if (plan.sendMqtt) {
    mqttSendData();
  }
  // HA reports on the MQTT interval
  if (plan.sendHa) {
    haSendData();
  }

  if (!plan.sleep) {
    return;
  }

  if (plan.disconnectMqtt) {
    mqttClient.disconnect();
    // Give the QoS-0 publishes just written and the DISCONNECT packet a moment
    // to actually leave the radio - they sit in the TCP buffer, and sleeping
    // now would drop them. Skipped when nothing was sent this wake (no link):
    // an unreachable-network wake is when the battery can least afford 250 ms
    // of standing around.
    if (plan.drainMqtt) {
      delay(MQTT_DRAIN_MS);
    }
  }
  // Say what the device will actually do: the plan's interval is the shortest
  // configured one, and the EEPROM log can be the one that decides it.
  D_print(F("Sleeping for (s): "));
  D_print(plan.sleepSeconds);
  if (tehybug.serveData.eeprom.active &&
      tehybug.serveData.eeprom.frequency == plan.sleepSeconds) {
    D_print(F("  (set by the EEPROM log interval)"));
  }
  D_println();

  startSleep(plan.sleepSeconds);
}

void checkScenario(Scenario &s) {
  if (!s.active) {
    return;
  }

  float val = 0;
  if (tehybug.sensorData.containsKey(s.data)) {
    val = tehybug.sensorData[s.data];
  }
  if (!mode_logic::scenarioConditionMet(s.condition, val, s.value)) {
    return;
  }

  D_println("condition met");
  D_println(s.url);
  if (s.type == "post") {
    // the URL is a template too, as it is for GET and for the main POST target
    const String url = tehybug.replacePlaceholders(s.url);
    http::post(httpClient, getClient(url), url,
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
