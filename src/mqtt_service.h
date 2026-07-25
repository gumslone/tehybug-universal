#pragma once
// MQTT publishing (plain and Home Assistant discovery).
//
// Expects the following globals (defined in tehybug.ino before this
// header is included): `tehybug`, `mqttClient`, `wifiSsid` — plus
// `Log()`/`getInfo()` from web_api.h and the `ha` namespace from ha.h.
#include <PubSubClient.h>
#include "debug.h"
#include "ha.h"

void mqttSendData();
void haSendData();
void mqttReconnect();

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

void haSendData() {
  if (mqttClient.connected()) {
    if (!haConfigPublished) {
      ha::publishAutoConfig(mqttClient, version, tehybug.sensorData);
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

// Subscribe to the remote-control topics. Without this the mqttCallback above
// could never fire — nothing ever subscribed, so getInfo/getConfig/setConfig
// over MQTT was dead. Only the three control topics are subscribed (not a
// wildcard) so the device does not receive its own info/config publishes back.
void mqttSubscribeControl() {
  const String &base = tehybug.serveData.mqtt.topic;
  mqttClient.subscribe((base + "getInfo").c_str());
  mqttClient.subscribe((base + "getConfig").c_str());
  mqttClient.subscribe((base + "setConfig").c_str());
  Log(F("MqttReconnect"), String(F("Subscribed to ")) + base + F("get/setConfig, getInfo"));
}

// One connection attempt per call, rate-limited to MQTT_RETRY_INTERVAL_MS.
// This used to loop internally with delay(5000) up to maxRetries — up to ~50 s
// of blocking inside a ticker callback, which stalled the web server, the
// websockets and the WiFi stack (and outlasted the 10 s MQTT keep-alive).
void mqttReconnect() {
  if (mqttClient.connected()) {
    return; // Already connected
  }

  MqttDataServ &mqtt = tehybug.serveData.mqtt;

  static unsigned long lastAttempt = 0;
  static bool attempted = false;
  const unsigned long now = millis();
  // unsigned subtraction, so this stays correct across the millis() rollover
  if (attempted && (now - lastAttempt) < MQTT_RETRY_INTERVAL_MS) {
    return;
  }
  lastAttempt = now;
  attempted = true;

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

    if (tehybug.serveData.ha.active) {
      haSendData();
    } else {
      mqttSendData();
    }
    return;
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
}
