#pragma once
// MQTT publishing (plain and Home Assistant discovery).
//
// Expects the following globals (defined in tehybug.ino before this
// header is included): `tehybug`, `mqttClient`, `wifiSsid` — plus
// `Log()`/`getInfo()` from web_api.h and the `ha` namespace from ha.h.
#include <PubSubClient.h>
#include "debug.h"
#include <FS.h>
#include "ha.h"

void mqttSendData();
void haSendData();
void mqttReconnect();
bool mqttEnsureConnected(unsigned long budgetMs);

void updateMqttClient() {
  if (tehybug.serveData.mqtt.active || tehybug.serveData.ha.active) {
    mqttClient.setServer(tehybug.serveData.mqtt.server.c_str(), tehybug.serveData.mqtt.port);
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  // Check the length before touching payload[0], and never write into the
  // buffer: it belongs to PubSubClient and is not NUL-terminated, so the old
  // `payload[length] = '\0'` wrote past the message (out of bounds when the
  // message filled the buffer). deserializeJson takes a pointer + length.
  if (length == 0 || payload[0] != '{') {
    return;
  }

  String channel = String(topic);
  channel.replace(tehybug.serveData.mqtt.topic, "");
  DynamicJsonDocument json(512);
  if (deserializeJson(json, payload, length)) {
    Log("MQTT_callback", "Malformed JSON on topic " + String(topic));
    return;
  }
  Log("MQTT_callback", "Incoming Json length to topic " + String(topic) +
      ": " + String(measureJson(json)));
  if (channel.equals("getInfo")) {
    mqttClient.publish((tehybug.serveData.mqtt.topic + "info").c_str(),
                       getInfo().c_str());
  } else if (channel.equals("getConfig")) {
    mqttClient.publish((tehybug.serveData.mqtt.topic + "config").c_str(),
                       tehybug.conf.getConfig().c_str());
  } else if (channel.equals("setConfig")) {
    // extract the data
    JsonObject object = json.as<JsonObject>();
    tehybug.conf.setConfig(object);
  }
}

// Home Assistant discovery is retained on the broker, so it only has to be
// published once per connection — it used to go out before every state update,
// which is one retained ~500-byte publish per sensor key on every reporting
// interval. Reset on (re)connect in mqttReconnect().
bool haConfigPublished = false;

#if !defined(ARDUINO_ESP8266_GENERIC)
// Deep sleep reboots the chip, so "once per connection" still means once per
// wake — one retained ~500-byte message per sensor key every cycle, which is
// radio time on a battery. Because the messages are retained, the broker keeps
// serving them while the device sleeps, so most of those wakes can skip it.
//
// The decision is remembered in RTC user memory: it survives deep sleep but is
// cleared by a power cycle, so reseating the battery republishes. Discovery is
// re-sent when the sensor set or the firmware version changes (the
// fingerprint), and at least every HA_DISCOVERY_MAX_WAKES wakes so a broker
// that lost its retained store recovers on its own.
struct HaDiscoveryMemo {
  uint32_t magic;
  uint32_t fingerprint;
  uint32_t wakes;
};
constexpr uint32_t HA_DISCOVERY_MAGIC = 0x48414443;  // 'HADC'
// Safety refresh window. Counted in seconds rather than wakes: at a 15 minute
// interval 96 wakes is a day, but at a 60 minute interval it would be four,
// and at a 10 second log interval a quarter of an hour.
constexpr uint32_t HA_DISCOVERY_REFRESH_S = 24UL * 60UL * 60UL;
constexpr uint32_t HA_DISCOVERY_RTC_SLOT = 64;       // dword offset, user area

// FNV-1a over the sensor keys plus the firmware version: a new sensor, or a
// build that renames/re-units one, changes the value and forces a republish.
uint32_t haDiscoveryFingerprint() {
  uint32_t h = 2166136261u;
  const auto mix = [&h](const char *s) {
    for (; s != nullptr && *s != '\0'; s++) {
      h ^= (uint8_t)*s;
      h *= 16777619u;
    }
  };
  for (JsonPair kv : tehybug.sensorData.as<JsonObject>()) {
    mix(kv.key().c_str());
  }
  mix(version.c_str());
  return h;
}

// Wakes that fit in the refresh window, given how often this device wakes.
uint32_t haDiscoveryMaxWakes() {
  const int interval = tehybug.wakeIntervalSeconds();
  if (interval <= 0) {
    return 1;
  }
  const uint32_t wakes = HA_DISCOVERY_REFRESH_S / (uint32_t)interval;
  return wakes < 1 ? 1 : wakes;
}

// True when discovery has to go out on this boot.
bool haDiscoveryNeeded() {
  HaDiscoveryMemo memo{};
  const uint32_t fingerprint = haDiscoveryFingerprint();

  if (ESP.rtcUserMemoryRead(HA_DISCOVERY_RTC_SLOT, (uint32_t *)&memo,
                            sizeof(memo)) &&
      memo.magic == HA_DISCOVERY_MAGIC && memo.fingerprint == fingerprint &&
      memo.wakes < haDiscoveryMaxWakes()) {
    memo.wakes++;
    ESP.rtcUserMemoryWrite(HA_DISCOVERY_RTC_SLOT, (uint32_t *)&memo,
                           sizeof(memo));
    D_print(F("HA discovery still retained, skipping (wake "));
    D_print(memo.wakes);
    D_println(F(")"));
    return false;
  }

  memo.magic = HA_DISCOVERY_MAGIC;
  memo.fingerprint = fingerprint;
  memo.wakes = 0;
  ESP.rtcUserMemoryWrite(HA_DISCOVERY_RTC_SLOT, (uint32_t *)&memo,
                         sizeof(memo));
  return true;
}
// Publishes discovery for the sensors present now, and retires the entities of
// any that have gone away.
//
// The published key set is kept in SPIFFS because retained discovery outlives
// the device: swap a sensor and, without this, its entity stays in Home
// Assistant showing the last value it ever reported.
constexpr const char *HA_KEYS_FILE = "/ha_keys.txt";

void haSyncDiscovery() {
  String previous;
  File in = SPIFFS.open(HA_KEYS_FILE, "r");
  if (in) {
    previous.reserve(in.size() + 1);
    while (in.available()) {
      previous += (char)in.read();
    }
    in.close();
  }

  // ",temp,humi," — wrapped in separators so a lookup cannot match a prefix
  // of a longer key ("temp" inside "temp2")
  String current = ",";
  for (JsonPair kv : tehybug.sensorData.as<JsonObject>()) {
    const String k = kv.key().c_str();
    if (k == "key") { // the device key is not a sensor
      continue;
    }
    current += k;
    current += ",";
  }

  // anything published before but missing now: tell HA to drop the entity
  int pos = 1;
  while (pos < (int)previous.length()) {
    const int next = previous.indexOf(',', pos);
    if (next < 0) {
      break;
    }
    const String k = previous.substring(pos, next);
    pos = next + 1;
    if (k.length() > 0 && current.indexOf("," + k + ",") < 0) {
      ha::removeAutoConfig(mqttClient, k);
    }
  }

  ha::publishAutoConfig(mqttClient, version, tehybug.sensorData);

  if (current != previous) {
    File out = SPIFFS.open(HA_KEYS_FILE, "w");
    if (out) {
      out.print(current);
      out.close();
    }
  }
}
#else
bool haDiscoveryNeeded() { return true; }
void haSyncDiscovery() { }
#endif

void haSendData() {
  if (mqttClient.connected()) {
    if (!haConfigPublished) {
      // consulted once per boot; the flag covers the rest of this session
      if (haDiscoveryNeeded()) {
        haSyncDiscovery();
      }
      haConfigPublished = true;
    }
    ha::publishState(mqttClient, tehybug.sensorData);
    Log(F("HomeAssistant"), F("haSendData"));
  } else {
    mqttReconnect();
  }
}

void mqttSendData() {
  if (!mqttClient.connected()) {
    Log(F("mqttSendData"), F("Not connected, reconnecting..."));
    mqttReconnect();
    return;
  }

  const String payload = tehybug.replacePlaceholders(tehybug.serveData.mqtt.message);

  Log(F("mqttSendData"), String(F("Topic: ")) + tehybug.serveData.mqtt.topic);
  Log(F("mqttSendData"), String(F("Payload: ")) + payload);

  const bool published = mqttClient.publish(tehybug.serveData.mqtt.topic.c_str(),
                                            payload.c_str(),
                                            tehybug.serveData.mqtt.retained);

  if (published) {
    Log(F("mqttSendData"), F("Published successfully"));
  } else {
    Log(F("mqttSendData"), F("Publish failed!"));
  }
}

// Wait between connection attempts. Not a delay(): see mqttReconnect().
constexpr unsigned long MQTT_RETRY_INTERVAL_MS = 5000;

// Sleep modes get one pass per wake, so they retry within a budget instead of
// waiting for a next loop iteration that never comes.
constexpr unsigned long MQTT_WAKE_CONNECT_BUDGET_MS = 6000;
constexpr unsigned long MQTT_WAKE_RETRY_GAP_MS = 400;

// Subscribe to the remote-control topics. Without this the mqttCallback above
// could never fire — nothing ever subscribed, so getInfo/getConfig/setConfig
// over MQTT was dead. Only the three control topics are subscribed (not a
// wildcard) so the device does not receive its own info/config publishes back.
void mqttSubscribeControl() {
  const String &base = tehybug.serveData.mqtt.topic;

  // Only for plain MQTT. The control topics hang off the user's master topic,
  // which only that mode configures — but the client is also brought up for
  // Home Assistant, which needs the same broker settings while leaving the
  // master topic untouched. Subscribing regardless meant an HA-only device
  // listened on a topic left over from an earlier MQTT setup.
  if (!tehybug.serveData.mqtt.active) {
    D_println(F("MQTT control topics skipped: plain MQTT is not active"));
    return;
  }
  if (base.length() == 0) {
    D_println(F("MQTT control topics skipped: no master topic set"));
    return;
  }

  mqttClient.subscribe((base + "getInfo").c_str());
  mqttClient.subscribe((base + "getConfig").c_str());
  mqttClient.subscribe((base + "setConfig").c_str());
  Log(F("MqttReconnect"), String(F("Subscribed to ")) + base +
      F("{getInfo,getConfig,setConfig}"));
}

// One connection attempt per call, rate-limited to MQTT_RETRY_INTERVAL_MS.
// This used to loop internally with delay(5000) up to maxRetries — up to ~50 s
// of blocking inside a ticker callback, which stalled the web server, the
// websockets and the WiFi stack (and outlasted the 10 s MQTT keep-alive).
// One connection attempt, no rate limiting. Returns true if it came up.
bool mqttAttemptConnect() {
  MqttDataServ &mqtt = tehybug.serveData.mqtt;

  Log(F("MqttReconnect"), F("Attempting connection..."));

  const char *availabilityTopic =
      tehybug.serveData.ha.active ? ha::MQTT_TOPIC_AVAILABILITY : "state";

  bool connected = false;
  if (mqtt.user.length() > 0 && mqtt.password.length() > 0) {
    connected = mqttClient.connect(wifiSsid, mqtt.user.c_str(),
                                   mqtt.password.c_str(), availabilityTopic,
                                   1, true, AVAILABILITY_OFFLINE);
  } else {
    connected = mqttClient.connect(wifiSsid, availabilityTopic, 1, true,
                                   AVAILABILITY_OFFLINE);
  }

  if (connected) {
    mqttClient.publish(availabilityTopic, AVAILABILITY_ONLINE, true);
    Log(F("MqttReconnect"), F("Connected!"));
    mqtt.retryCounter = 0;
    mqttSubscribeControl();
    // a fresh session (and any broker restart behind it) needs discovery again
    haConfigPublished = false;

    // Deliberately no send here: the caller (serve_data on the sleep path, the
    // ticker in live mode) publishes right after. Sending on connect as well
    // published every value twice.
    return true;
  }

  mqtt.retryCounter++;
  Log(F("MqttReconnect"), String(F("Failed, rc=")) + String(mqttClient.state()));
  Log(F("MqttReconnect"), String(F("Retry ")) + String(mqtt.retryCounter) +
      String(F("/")) + String(mqtt.maxRetries));
  updateMqttClient();

  if (mqtt.retryCounter >= mqtt.maxRetries) {
    Log(F("MqttReconnect"), F("Max retries reached, MQTT deactivated"));
    if (!tehybug.sleepEnabled()) {
      ESP.restart();
    }
  }
  return false;
}

// Live mode: one attempt per call, rate-limited, because loop() comes back
// around and will try again.
void mqttReconnect() {
  if (mqttClient.connected()) {
    return; // Already connected
  }

  static unsigned long lastAttempt = 0;
  static bool attempted = false;
  const unsigned long now = millis();
  // unsigned subtraction, so this stays correct across the millis() rollover
  if (attempted && (now - lastAttempt) < MQTT_RETRY_INTERVAL_MS) {
    return;
  }
  lastAttempt = now;
  attempted = true;

  mqttAttemptConnect();
}

// Sleep modes: keep trying within a budget, because there is no next loop
// iteration — the device sends once and sleeps, so a single failed attempt
// silently drops that cycle's reading.
//
// The first attempt after a wake fails routinely: the stack is rebuilt from
// scratch on a deep-sleep boot and can reuse the source port of the previous
// session while the broker still holds that connection in TIME_WAIT, so the
// SYN goes nowhere (PubSubClient reports rc=-2, a plain TCP failure, even
// though the broker is reachable). A second attempt gets a different port and
// succeeds, which is why every other wake appeared to fail.
bool mqttEnsureConnected(unsigned long budgetMs) {
  const unsigned long start = millis();
  uint8_t attempt = 0;
  MqttDataServ &mqtt = tehybug.serveData.mqtt;
  while (!mqttClient.connected() && (millis() - start) < budgetMs) {
    if (mqtt.retryCounter >= mqtt.maxRetries) {
      Log(F("MqttReconnect"), F("Giving up for this cycle"));
      break;
    }
    attempt++;
    if (mqttAttemptConnect()) {
      if (attempt > 1) {
        D_print(F("MQTT connected on attempt "));
        D_println(attempt);
      }
      return true;
    }
    delay(MQTT_WAKE_RETRY_GAP_MS);
    yield();
  }
  return mqttClient.connected();
}
