#pragma once
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include "debug.h"
#include "common_functions.h"

#define AVAILABILITY_ONLINE "online"
#define AVAILABILITY_OFFLINE "offline"

namespace ha {
  char identifier[16];
  char MQTT_TOPIC_STATE[64];
  char MQTT_TOPIC_AVAILABILITY[64];

void setupHandle() {
  const String deviceName = "TEHYBUG";
  snprintf(identifier, sizeof(identifier), "%s-%X", deviceName.c_str(), ESP.getChipId());
  // sizeof, not a hardcoded 63, so these stay correct if the buffers change
  snprintf(MQTT_TOPIC_STATE, sizeof(MQTT_TOPIC_STATE), "%s/%s/state",
           deviceName.c_str(), identifier);
  snprintf(MQTT_TOPIC_AVAILABILITY, sizeof(MQTT_TOPIC_AVAILABILITY),
           "%s/%s/status", deviceName.c_str(), identifier);
}

// Home Assistant MQTT discovery + state.
//
// Compiled out of the generic (1MB) build for old / first-generation boards:
// it costs ~6 KB of flash, and that build sits just under the size at which it
// can still update itself over the air. Keeping OTA working there matters more
// than HA support, so these become no-ops (like the RTC/EEPROM and https
// features that build already omits). The ESP8285 builds are unaffected.

 void publishAutoConfig(PubSubClient & mqttClient, const String & version, DynamicJsonDocument & sensorData) {
#if !defined(ARDUINO_ESP8266_GENERIC)
  char mqttPayload[1024];
  DynamicJsonDocument device(256);
  DynamicJsonDocument autoconfPayload(1024);
  StaticJsonDocument<64> identifiersDoc;
  JsonArray identifiers = identifiersDoc.to<JsonArray>();
  identifiers.add(identifier);
  
  device["identifiers"] = identifiers;
  device["manufacturer"] = "TeHyBug";
  device["model"] = "TeHyBug Universal/Mini";
  device["name"] = identifier;
  device["sw_version"] = version;
  
  {
    const String wifiTopic = "homeassistant/sensor/"+String(identifier)+"/"+String(identifier)+"_wifi/config";  
    autoconfPayload["device"] = device.as<JsonObject>();
    autoconfPayload["state_topic"] = MQTT_TOPIC_STATE;
    autoconfPayload["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
    autoconfPayload["name"] = "WiFi";
    autoconfPayload["value_template"] = "{{value_json.wifi.rssi}}";
    autoconfPayload["unique_id"] = String(identifier) + "_wifi";
    autoconfPayload["unit_of_measurement"] = "dBm";
    autoconfPayload["json_attributes_topic"] = MQTT_TOPIC_STATE;
    autoconfPayload["json_attributes_template"] = "{\"ssid\": \"{{value_json.wifi.ssid}}\", \"ip\": \"{{value_json.wifi.ip}}\"}";
    autoconfPayload["icon"] = "mdi:wifi";
    serializeJson(autoconfPayload, mqttPayload);
    mqttClient.publish(wifiTopic.c_str(), &mqttPayload[0], true);
    autoconfPayload.clear();
  }
  const JsonObject root = sensorData.as<JsonObject>();
  for (JsonPair keyValue : root) {
    const String k = keyValue.key().c_str();
    if(k == "key")
     continue;
    const String v = keyValue.value();
    const String topic = "homeassistant/sensor/"+String(identifier)+"/"+String(identifier)+"_"+k+"/config";
  
    autoconfPayload["device"] = device.as<JsonObject>();
    autoconfPayload["state_topic"] = MQTT_TOPIC_STATE;
    autoconfPayload["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
    autoconfPayload["name"] = key2name(k);
    autoconfPayload["value_template"] = "{{value_json." + k + "}}";
    if(k != "cs" && k != "cs2")
      autoconfPayload["unit_of_measurement"] = key2unit(k);
    autoconfPayload["icon"] = key2icon(k);
    autoconfPayload["unique_id"] = String(identifier) + "_sensor_" + k;
    serializeJson(autoconfPayload, mqttPayload);
    mqttClient.publish(topic.c_str(), &mqttPayload[0], true);
    autoconfPayload.clear();
  }
  device.clear();
  identifiersDoc.clear();
#endif
}

// Removes a sensor's entity from Home Assistant.
//
// Discovery messages are retained per topic, so publishing a new set never
// clears the old one: unplug a sensor and its entity lingers in HA forever,
// showing the last value it ever reported. An empty retained payload on the
// same config topic is how HA is told to drop it.
void removeAutoConfig(PubSubClient & mqttClient, const String & key) {
#if !defined(ARDUINO_ESP8266_GENERIC)
  const String topic = "homeassistant/sensor/" + String(identifier) + "/" +
                       String(identifier) + "_" + key + "/config";
  mqttClient.publish(topic.c_str(), "", true);
  D_print(F("HA entity removed: "));
  D_println(key);
#endif
}

void publishState(PubSubClient & mqttClient, DynamicJsonDocument & sensorData) {
#if !defined(ARDUINO_ESP8266_GENERIC)
  DynamicJsonDocument wifiJson(192);
  // Was 512 with a 256-byte stack buffer: a normal two-sensor set overflows
  // both, so Home Assistant received silently truncated (invalid) JSON.
  DynamicJsonDocument stateJson(1024);

  wifiJson["ssid"] = WiFi.SSID();
  wifiJson["ip"] = WiFi.localIP().toString();
  wifiJson["rssi"] = WiFi.RSSI();

  stateJson["wifi"] = wifiJson.as<JsonObject>();

  const JsonObject root = sensorData.as<JsonObject>();
  for (JsonPair keyValue : root) {
    const String k = keyValue.key().c_str();
    if(k == "key")
     continue;
    if(k == "cs"||k == "cs2")
      stateJson[k] = cf2name(keyValue.value().as<int>());
    else
      stateJson[k] = keyValue.value().as<double>();
  }

  if (stateJson.overflowed()) {
    D_println(F("HA state JSON overflowed, publishing anyway"));
  }
  // Serialize into a String, which grows to fit — the old fixed 256-byte buffer
  // silently truncated a normal sensor set into invalid JSON. A String (rather
  // than streaming to the client as a Print) reuses the serializer instantiation
  // the config/websocket paths already pull in, which matters because the 1MB
  // generic build is close to its OTA size ceiling.
  String payload;
  payload.reserve(measureJson(stateJson) + 1);
  serializeJson(stateJson, payload);
  if (!mqttClient.publish(&MQTT_TOPIC_STATE[0], payload.c_str(), true)) {
    D_println(F("HA state publish failed"));
  }
  stateJson.clear();
  wifiJson.clear();
#endif
}

}
