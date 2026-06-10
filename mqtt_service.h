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
  if (payload[0] == '{') {
    payload[length] = '\0';
    String channel = String(topic);
    channel.replace(tehybug.serveData.mqtt.topic, "");
    DynamicJsonDocument json(512);
    deserializeJson(json, payload);
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
}

void haSendData() {
  if (mqttClient.connected()) {
    ha::publishAutoConfig(mqttClient, version, tehybug.sensorData);
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

void mqttReconnect() {
  if (mqttClient.connected()) {
    return; // Already connected
  }

  MqttDataServ &mqtt = tehybug.serveData.mqtt;

  while (!mqttClient.connected() && mqtt.retryCounter < mqtt.maxRetries) {

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

      if (tehybug.serveData.ha.active) {
        haSendData();
      } else {
        mqttSendData();
      }
      break;
    }

    mqtt.retryCounter++;
    Log(F("MqttReconnect"), String(F("Failed, rc=")) + String(mqttClient.state()));
    Log(F("MqttReconnect"), String(F("Retry ")) + String(mqtt.retryCounter) +
        String(F("/")) + String(mqtt.maxRetries));
    delay(5000);
    updateMqttClient();
  }

  if (mqtt.retryCounter >= mqtt.maxRetries) {
    Log(F("MqttReconnect"), F("Max retries reached, MQTT deactivated"));
    if (!tehybug.sleepEnabled()) {
      ESP.restart();
    }
  }
}
