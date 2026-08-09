#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include "debug.h"
#include "data_types.h"
#include "pixel.h"

class TeHyBugConfig {
  public:

    TeHyBugConfig(Calibration & calibration, Sensor & sensor, Peripherals & peripherals, Device & device, DataServ & serveData, Scenarios & scenarios, DisplayConf & displayConf, Alarms & alarms, TeHyBugPixel & pixel) :
      m_calibration(calibration),
      m_sensor(sensor),
      m_peripherals(peripherals),
      m_device(device),
      m_serveData(serveData),
      m_scenarios(scenarios),
      m_displayConf(displayConf),
      m_alarms(alarms),
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

      DynamicJsonDocument json(3072);
      buildConfig(json, false); // only non-defaults: keeps the flash file small

      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        // "w" already truncated nothing (the open failed), but the caller must
        // not be told the settings were stored — this used to be ignored, so a
        // full or failed filesystem silently lost the configuration.
        D_println(F("Config save failed: cannot open /config.json"));
        return;
      }
      const size_t written = serializeJson(json, configFile);
      configFile.close();
      if (written == 0) {
        D_println(F("Config save failed: nothing written"));
        return;
      }
      m_shouldSaveConfig = false; // stored; nothing pending until the next change
      D_println(F("Config saved"));
    }

    // Builds the config document. full=false writes only values that differ
    // from the compiled-in defaults, which keeps the flash file small.
    // full=true writes every key: the UI needs that, because a value equal to
    // its default is still a value - omitting it left fields showing
    // "Loading or no data" and made a deliberately-set default look unsaved
    // (set the MQTT topic to the default string and it "disappeared").
    void buildConfig(DynamicJsonDocument &json, bool full) {
      // default-constructed structs provide the values we may omit
      const Calibration calibration{};
      const Sensor sensor{};
      const Device device{};
      const DataServ serveData{};
      const Scenario scenario{};

      json["key"] = m_device.key;
      // Which board this binary targets — for the UI dump only, never
      // persisted (it is a build fact, not a setting). The web UI uses it to
      // show board-specific pages (the Display page) only where they apply.
      if (full) {
        json["board"] = BOARD_NAME;
      }


      put(json, full, "mqttActive", m_serveData.mqtt.active, serveData.mqtt.active);
      put(json, full, "mqttRetained", m_serveData.mqtt.retained, serveData.mqtt.retained);
      put(json, full, "mqttUser", m_serveData.mqtt.user, serveData.mqtt.user);
      put(json, full, "mqttPassword", m_serveData.mqtt.password, serveData.mqtt.password);
      put(json, full, "mqttServer", m_serveData.mqtt.server, serveData.mqtt.server);
      put(json, full, "mqttMasterTopic", m_serveData.mqtt.topic, serveData.mqtt.topic);
      put(json, full, "mqttMessage", m_serveData.mqtt.message,  serveData.mqtt.message);
      put(json, full, "mqttPort", m_serveData.mqtt.port, serveData.mqtt.port);
      put(json, full, "mqttFrequency", m_serveData.mqtt.frequency, serveData.mqtt.frequency);

      put(json, full, "haActive", m_serveData.ha.active, serveData.ha.active);

      put(json, full, "eepromLogActive", m_serveData.eeprom.active, serveData.eeprom.active);
      put(json, full, "eepromLogFrequency", m_serveData.eeprom.frequency, serveData.eeprom.frequency);
      put(json, full, "eepromLogMessage", m_serveData.eeprom.message, serveData.eeprom.message);
      put(json, full, "eepromLogHourly", m_serveData.eeprom.hourly, serveData.eeprom.hourly);
      put(json, full, "offlineModeActive", m_device.offlineMode, device.offlineMode);

      put(json, full, "httpGetURL", m_serveData.get.url,  serveData.get.url);
      put(json, full, "httpGetActive", m_serveData.get.active, serveData.get.active);
      put(json, full, "httpGetFrequency", m_serveData.get.frequency, serveData.get.frequency);

      put(json, full, "httpPostURL", m_serveData.post.url, serveData.post.url);
      put(json, full, "httpPostActive", m_serveData.post.active, serveData.post.active);
      put(json, full, "httpPostFrequency", m_serveData.post.frequency, serveData.post.frequency);
      put(json, full, "httpPostJson", m_serveData.post.message, serveData.post.message);

      put(json, full, "calibrationActive", m_calibration.active, calibration.active);
      put(json, full, "calibrationTemp",  m_calibration.temp, calibration.temp);
      put(json, full, "calibrationHumi", m_calibration.humi, calibration.humi);
      put(json, full, "calibrationQfe", m_calibration.qfe, calibration.qfe);

      put(json, full, "configModeActive", m_device.configMode, device.configMode); // true by default
      put(json, full, "sleepModeActive", m_device.sleepMode, device.sleepMode);
      put(json, full, "lightSleepModeActive", m_device.lightSleepMode, device.lightSleepMode);

      put(json, full, "dht_sensor", m_sensor.dht, sensor.dht);
      put(json, full, "second_dht_sensor", m_sensor.dht_2, sensor.dht_2);

      put(json, full, "ds18b20_sensor", m_sensor.ds18b20, sensor.ds18b20);
      put(json, full, "second_ds18b20_sensor", m_sensor.ds18b20_2, sensor.ds18b20_2);
      put(json, full, "adc_sensor", m_sensor.adc, sensor.adc);

      for (uint8_t i = 0; i < Scenarios::count; i++) {
        const String prefix = "sc" + String(i + 1) + "_";
        Scenario &sc = m_scenarios.items[i];
        put(json, full, prefix + "active", sc.active, scenario.active);
        put(json, full, prefix + "type", sc.type, scenario.type);
        put(json, full, prefix + "url", sc.url, scenario.url);
        put(json, full, prefix + "data", sc.data, scenario.data);
        put(json, full, prefix + "condition", sc.condition, scenario.condition);
        put(json, full, prefix + "value", sc.value, scenario.value);
        put(json, full, prefix + "message", sc.message, scenario.message);
      }

      put(json, full, "rc_active", m_device.remoteControl.active, device.remoteControl.active);
      put(json, full, "rc_url", m_device.remoteControl.url, device.remoteControl.url);

#if TEHYBUG_DISPLAY
      // Display board only. The key names are the original display
      // firmware's, so a device upgraded from it keeps its stored pages,
      // clock options and alarms.
      const DisplayConf displayConf{};
      const AlarmConf alarmConf{};
      put(json, full, "line1", m_displayConf.line1, displayConf.line1);
      put(json, full, "line2", m_displayConf.line2, displayConf.line2);
      put(json, full, "line3", m_displayConf.line3, displayConf.line3);
      put(json, full, "clock_12h", m_displayConf.clock12h, displayConf.clock12h);
      put(json, full, "clock_show_ip", m_displayConf.showIp, displayConf.showIp);
      put(json, full, "clock_sleep", m_displayConf.nightMode, displayConf.nightMode);
      put(json, full, "clock_sleep_start", m_displayConf.nightStart, displayConf.nightStart);
      put(json, full, "clock_sleep_finish", m_displayConf.nightEnd, displayConf.nightEnd);
      for (uint8_t i = 0; i < Alarms::count; i++) {
        const String prefix = "alarm" + String(i + 1);
        AlarmConf &alarm = m_alarms.items[i];
        put(json, full, prefix + "Active", alarm.active, alarmConf.active);
        put(json, full, prefix + "Time", alarm.time, alarmConf.time);
        put(json, full, prefix + "Message", alarm.message, alarmConf.message);
        put(json, full, prefix + "Weekdays", alarm.weekdays, alarmConf.weekdays);
      }
#endif
    }

    // Smallest reporting interval accepted. A read + send pass can hold the
    // loop for several seconds (a DHT sample alone takes ~2 s each, an
    // unreachable HTTP target up to the request timeout). With a shorter
    // interval than that the ticker re-fires as soon as it returns, starving
    // loop() so the web server and MQTT keep-alive never run — the device
    // looks frozen. 0 would fire continuously.
    static constexpr int MIN_DATA_FREQUENCY_S = 10;

    void validateDataFrequency(int &freq) {
      const int maxDS = (int)(ESP.deepSleepMax() / 1000000);
      if (freq > maxDS) {
        freq = maxDS;
      }
      if (freq < MIN_DATA_FREQUENCY_S) {
        freq = MIN_DATA_FREQUENCY_S;
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

    // Serves the complete configuration, not the stored file: the file only
    // holds non-default values, and the UI must also see the ones that equal
    // their defaults (see buildConfig). Costs one 3 KB document per config
    // page load, paid only in config mode where the heap is at its freest.
    String getConfig() {
      DynamicJsonDocument json(3072);
      buildConfig(json, true);
      String out;
      out.reserve(measureJson(json) + 1);
      serializeJson(json, out);
      return out;
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
    DisplayConf & m_displayConf;
    Alarms & m_alarms;
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

      setData(json, "eepromLogActive", m_serveData.eeprom.active);
      setFrequency(json, "eepromLogFrequency", m_serveData.eeprom.frequency);
      setData(json, "eepromLogMessage", m_serveData.eeprom.message);
      setData(json, "eepromLogHourly", m_serveData.eeprom.hourly);
      setData(json, "offlineModeActive", m_device.offlineMode);

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
      // saveConfig() writes "key", so it must be read back here too — without
      // this the stored device key was ignored and regenerated on every boot.
      setData(json, "key", m_device.key);

#if TEHYBUG_DISPLAY
      setData(json, "line1", m_displayConf.line1);
      setData(json, "line2", m_displayConf.line2);
      setData(json, "line3", m_displayConf.line3);
      setData(json, "clock_12h", m_displayConf.clock12h);
      setData(json, "clock_show_ip", m_displayConf.showIp);
      setData(json, "clock_sleep", m_displayConf.nightMode);
      setData(json, "clock_sleep_start", m_displayConf.nightStart);
      setData(json, "clock_sleep_finish", m_displayConf.nightEnd);
      for (uint8_t i = 0; i < Alarms::count; i++) {
        const String prefix = "alarm" + String(i + 1);
        AlarmConf &alarm = m_alarms.items[i];
        setData(json, prefix + "Active", alarm.active);
        setData(json, prefix + "Time", alarm.time);
        setData(json, prefix + "Message", alarm.message);
        setData(json, prefix + "Weekdays", alarm.weekdays);
      }
      // The original display firmware stored "wifiActive"; its false is this
      // firmware's offline mode, so an upgraded device that was running
      // display-only keeps doing that instead of silently joining WiFi.
      if (json.containsKey("wifiActive") && !json["wifiActive"].as<bool>()) {
        m_device.offlineMode = true;
      }

      // A display board is mains powered and its panel must keep drawing:
      // ignore sleep modes a shared or upgraded config may carry, and drop
      // the Port B pin sensors — GPIO0/GPIO2 are the OLED's I2C bus here.
      m_device.sleepMode = false;
      m_device.lightSleepMode = false;
      m_sensor.dht = false;
      m_sensor.ds18b20 = false;
#endif
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

    // One writer for both config shapes: full=true writes unconditionally
    // (the UI dump), full=false only when the value differs from its
    // compiled-in default (the flash file).
    template<typename T>
    void put(DynamicJsonDocument &json, bool full, const String& key, const T & var, const T & defaultVar)
    {
      if (full || var != defaultVar) {
        json[key] = var;
      }
    }

}; // class Config
