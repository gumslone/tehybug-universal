#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include "debug.h"
#include "data_types.h"
#include "pixel.h"

class TeHyBugConfig {
  public:

    TeHyBugConfig(Calibration & calibration, Sensor & sensor, Peripherals & peripherals, Device & device, DataServ & serveData, Scenarios & scenarios, TeHyBugPixel & pixel) :
      m_calibration(calibration),
      m_sensor(sensor),
      m_peripherals(peripherals),
      m_device(device),
      m_serveData(serveData),
      m_scenarios(scenarios),
      m_pixel(pixel)
    {}
    void saveConfigCallback() {
      m_shouldSaveConfig = true;
    }
    void saveConfig(bool force = false) {
      // save the custom parameters to FS
      if (!m_shouldSaveConfig && !force) {
        return;
      }

      // default-constructed structs provide the values we can omit from the file
      const Calibration calibration{};
      const Sensor sensor{};
      const Device device{};
      const DataServ serveData{};
      const Scenario scenario{};

      DynamicJsonDocument json(3072);

      json["key"] = m_device.key;

      setIfNotDefault(json, "mqttActive", m_serveData.mqtt.active, serveData.mqtt.active);
      setIfNotDefault(json, "mqttRetained", m_serveData.mqtt.retained, serveData.mqtt.retained);
      setIfNotDefault(json, "mqttUser", m_serveData.mqtt.user, serveData.mqtt.user);
      setIfNotDefault(json, "mqttPassword", m_serveData.mqtt.password, serveData.mqtt.password);
      setIfNotDefault(json, "mqttServer", m_serveData.mqtt.server, serveData.mqtt.server);
      setIfNotDefault(json, "mqttMasterTopic", m_serveData.mqtt.topic, serveData.mqtt.topic);
      setIfNotDefault(json, "mqttMessage", m_serveData.mqtt.message,  serveData.mqtt.message);
      setIfNotDefault(json, "mqttPort", m_serveData.mqtt.port, serveData.mqtt.port);
      setIfNotDefault(json, "mqttFrequency", m_serveData.mqtt.frequency, serveData.mqtt.frequency);

      setIfNotDefault(json, "haActive", m_serveData.ha.active, serveData.ha.active);

      setIfNotDefault(json, "httpGetURL", m_serveData.get.url,  serveData.get.url);
      setIfNotDefault(json, "httpGetActive", m_serveData.get.active, serveData.get.active);
      setIfNotDefault(json, "httpGetFrequency", m_serveData.get.frequency, serveData.get.frequency);

      setIfNotDefault(json, "httpPostURL", m_serveData.post.url, serveData.post.url);
      setIfNotDefault(json, "httpPostActive", m_serveData.post.active, serveData.post.active);
      setIfNotDefault(json, "httpPostFrequency", m_serveData.post.frequency, serveData.post.frequency);
      setIfNotDefault(json, "httpPostJson", m_serveData.post.message, serveData.post.message);

      setIfNotDefault(json, "calibrationActive", m_calibration.active, calibration.active);
      setIfNotDefault(json, "calibrationTemp",  m_calibration.temp, calibration.temp);
      setIfNotDefault(json, "calibrationHumi", m_calibration.humi, calibration.humi);
      setIfNotDefault(json, "calibrationQfe", m_calibration.qfe, calibration.qfe);

      setIfNotDefault(json, "configModeActive", m_device.configMode, device.configMode); // true by default
      setIfNotDefault(json, "sleepModeActive", m_device.sleepMode, device.sleepMode);
      setIfNotDefault(json, "lightSleepModeActive", m_device.lightSleepMode, device.lightSleepMode);

      setIfNotDefault(json, "dht_sensor", m_sensor.dht, sensor.dht);
      setIfNotDefault(json, "second_dht_sensor", m_sensor.dht_2, sensor.dht_2);

      setIfNotDefault(json, "ds18b20_sensor", m_sensor.ds18b20, sensor.ds18b20);
      setIfNotDefault(json, "second_ds18b20_sensor", m_sensor.ds18b20_2, sensor.ds18b20_2);
      setIfNotDefault(json, "adc_sensor", m_sensor.adc, sensor.adc);

      for (uint8_t i = 0; i < Scenarios::count; i++) {
        const String prefix = "sc" + String(i + 1) + "_";
        Scenario &sc = m_scenarios.items[i];
        setIfNotDefault(json, prefix + "active", sc.active, scenario.active);
        setIfNotDefault(json, prefix + "type", sc.type, scenario.type);
        setIfNotDefault(json, prefix + "url", sc.url, scenario.url);
        setIfNotDefault(json, prefix + "data", sc.data, scenario.data);
        setIfNotDefault(json, prefix + "condition", sc.condition, scenario.condition);
        setIfNotDefault(json, prefix + "value", sc.value, scenario.value);
        setIfNotDefault(json, prefix + "message", sc.message, scenario.message);
      }

      setIfNotDefault(json, "rc_active", m_device.remoteControl.active, device.remoteControl.active);
      setIfNotDefault(json, "rc_url", m_device.remoteControl.url, device.remoteControl.url);

      File configFile = SPIFFS.open("/config.json", "w");
      serializeJson(json, configFile);
      configFile.close();
      D_println(F("Config saved"));
    }

    void validateDataFrequency(int &freq) {
      const int maxDS = (int)(ESP.deepSleepMax() / 1000000);
      if (freq > maxDS) {
        freq = maxDS;
      }
    }
    bool configExists() {
      return SPIFFS.exists("/config.json");
    }

    void loadConfig() {
      if (configExists()) {
        // file exists, reading and loading
        File configFile = SPIFFS.open("/config.json", "r");

        if (configFile) {
          D_println(F("opened config file"));

          DynamicJsonDocument json(3072);
          const auto error = deserializeJson(json, configFile);

          if (!error) {
            JsonObject documentRoot = json.as<JsonObject>();
            setConfigParameters(documentRoot);

            D_println(F("Config loaded"));
          } else {
            D_print(F("Deserialization failed: "));
            D_println(error.c_str());
          }
        }
      } else {
        D_println(F("No configfile found, create a new file"));
        m_firstStart = true; //probably the device was factory reset or new
        saveConfig(true);
      }
    }

    void setConfig(JsonObject &json) {
      setConfigParameters(json);
      saveConfig(true);

      // restart the module when reboot is requested in save config
      if (json.containsKey("reboot") && json["reboot"]) {
        m_pixel.off();
        yield();
        delay(1000);
        ESP.restart();
      }
    }

    String getConfig() {
      String json = "{}";
      File configFile = SPIFFS.open("/config.json", "r");

      if (configFile) {
        DynamicJsonDocument root(3072);
        if (DeserializationError::Ok == deserializeJson(root, configFile)) {
          json = "";
          serializeJson(root, json);
        }
        configFile.close();
      }
      return json;
    }
    bool firstStart()
    {
      return m_firstStart;
    }

    bool rtcActive()
    {
      return m_peripherals.ds3231;
    }
    bool eepromActive()
    {
      return m_peripherals.eeprom;
    }

  private:

    bool m_shouldSaveConfig{false};
    bool m_firstStart{false};
    Calibration & m_calibration;
    Sensor & m_sensor;
    Device & m_device;
    DataServ & m_serveData;
    Scenarios & m_scenarios;
    Peripherals & m_peripherals;
    TeHyBugPixel & m_pixel;

    void setConfigParameters(const JsonObject &json) {
      D_println("Config:");
      if (DEBUG) {
        for (JsonPair kv : json) {
          D_print(kv.key().c_str());
          D_print(" = ");
          D_println(kv.value().as<String>());
        }
        D_println();
      }

      setData(json, "mqttActive", m_serveData.mqtt.active);
      setData(json, "mqttRetained", m_serveData.mqtt.retained);
      setData(json, "mqttUser", m_serveData.mqtt.user);
      setData(json, "mqttPassword", m_serveData.mqtt.password);
      setData(json, "mqttServer", m_serveData.mqtt.server);
      setData(json, "mqttMasterTopic", m_serveData.mqtt.topic);
      setData(json, "mqttMessage", m_serveData.mqtt.message);
      setData(json, "mqttPort", m_serveData.mqtt.port);
      setFrequency(json, "mqttFrequency", m_serveData.mqtt.frequency);

      setData(json, "haActive", m_serveData.ha.active);

      // http
      setData(json, "httpGetURL", m_serveData.get.url);
      setData(json, "httpGetActive", m_serveData.get.active);
      setFrequency(json, "httpGetFrequency", m_serveData.get.frequency);

      setData(json, "httpPostURL", m_serveData.post.url);
      setData(json, "httpPostActive", m_serveData.post.active);
      setFrequency(json, "httpPostFrequency", m_serveData.post.frequency);

      setData(json, "httpPostJson", m_serveData.post.message);
      setData(json, "configModeActive", m_device.configMode);
      setData(json, "calibrationActive", m_calibration.active);
      setData(json, "calibrationTemp", m_calibration.temp);
      setData(json, "calibrationHumi", m_calibration.humi);
      setData(json, "calibrationQfe", m_calibration.qfe);
      setData(json, "lightSleepModeActive", m_device.lightSleepMode);
      setData(json, "sleepModeActive", m_device.sleepMode);
      setData(json, "dht_sensor", m_sensor.dht);
      setData(json, "second_dht_sensor", m_sensor.dht_2);
      setData(json, "ds18b20_sensor", m_sensor.ds18b20);
      setData(json, "second_ds18b20_sensor", m_sensor.ds18b20_2);
      setData(json, "adc_sensor", m_sensor.adc);

      for (uint8_t i = 0; i < Scenarios::count; i++) {
        const String prefix = "sc" + String(i + 1) + "_";
        Scenario &sc = m_scenarios.items[i];
        setData(json, prefix + "active", sc.active);
        setData(json, prefix + "type", sc.type);
        setData(json, prefix + "url", sc.url);
        setData(json, prefix + "data", sc.data);
        setData(json, prefix + "condition", sc.condition);
        setData(json, prefix + "value", sc.value);
        setData(json, prefix + "message", sc.message);
      }

      setData(json, "rc_active", m_device.remoteControl.active);
      setData(json, "rc_url", m_device.remoteControl.url);
    }

    template<typename T>
    void setData(const JsonObject &json, const String& key, T & var)
    {
      if (json.containsKey(key)) {
        var = json[key].as<T>();
      }
    }

    void setFrequency(const JsonObject &json, const String& key, int & freq)
    {
      if (json.containsKey(key)) {
        freq = json[key].as<int>();
        validateDataFrequency(freq);
      }
    }

    template<typename T>
    void setIfNotDefault(DynamicJsonDocument &json, const String& key, const T & var, const T & defaultVar)
    {
      if(var != defaultVar)
      {
        json[key] = var;
      }
    }

}; // class Config
