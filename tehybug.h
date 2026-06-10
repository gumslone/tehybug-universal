#pragma once
#include <climits>
#include <ArduinoJson.h>
#include "DHTesp.h"
#include "debug.h"
#include "common_functions.h"
#include "data_types.h"
#include "pixel.h"
#include "configuration.h"
#include "UUID.h"
#include "rtc_time.h"
#include "eeprom.h"

class TeHyBug {
  public:
    Calibration calibration{};
    Sensor sensor{};
    Device device{};
    Peripherals peripherals{};
    DataServ serveData{};
    Scenarios scenarios{};
    DynamicJsonDocument sensorData;
    TeHyBugConfig conf;
    RtcTime time;
    TeHyBugEeprom eeprom;
    TeHyBugPixel pixel;

    TeHyBug(DHTesp & dht): sensorData(1024), m_dht(dht), conf(calibration, sensor, peripherals, device, serveData, scenarios, pixel), time(conf), eeprom(time) {
    }

    String replacePlaceholders(String text) {
      const JsonObject root = sensorData.as<JsonObject>();
      for (JsonPair keyValue : root) {
        String k = keyValue.key().c_str();
        String v = keyValue.value();
        text.replace("%" + k + "%", v);
      }
      return text;
    }

    void additionalSensorData(const String & key, const float & value) {

      if (key == "temp" || key == "temp2") {
        addSensorData(key + "_imp", temp2Imp(value));
      }
      // humi should be always set after temp so the following calculation will work
      else if (key == "humi" || key == "humi2") {

        const String num = (key == "humi2") ? "2" : "";
        const float temp = sensorData["temp" + num].as<float>();

        const float hi = m_dht.computeHeatIndex(temp, value);
        addSensorData("hi" + num, hi);
        addSensorData("hi_imp" + num, temp2Imp(hi));

        const float dew = m_dht.computeDewPoint(temp, value);
        addSensorData("dew" + num, dew);
        addSensorData("dew_imp" + num, temp2Imp(dew));

        const float ah = m_dht.computeAbsoluteHumidity(temp, value);
        addSensorData("ah" + num, ah);

        ComfortState cs;
        const float cr = m_dht.getComfortRatio(cs, temp, value, false);
        addSensorData("cr" + num, cr);
        addSensorData("cs" + num, (int)cs);
      }
    }

    void addSensorData(const String & key, float value) {
      value = calibrateValue(key, value);
      sensorData[key] = String(value, 1);
      // calculate imperial temperature also heat index and the dew point
      additionalSensorData(key, value);
    }
    void addSensorData(const String & key, int value) {
      sensorData[key] = String(value);
    }
    void addTempHumi(const String & key_temp, float temp, const String & key_humi, float humi) {
      addSensorData(key_temp, temp);
      addSensorData(key_humi, humi);
    }

    void finalizeLoop() {
      if(m_sensorDataGarbageCollect) {
        sensorDataGarbageCollect();
      }
    }

    void getDeviceKey() {
      // UUID – is a 36-character alphanumeric string
      String key = device.key;
      if (key.length() != 36) {
        key = generateDeviceKey();
        setDeviceKey(key);
      }
      D_print(F("key: "));
      D_println(key);
    }

    void handleRemoteControl(const String & data )
    {
      DynamicJsonDocument json(4096);
      const auto error = deserializeJson(json, data);
      if (!error) {
        JsonObject root = json.as<JsonObject>();
          if (json.containsKey("configMode")) {
            if (root["configMode"])
            {
              device.configMode = true;
              tickerStop = true;
            }
            else
            {
              device.configMode = false;
              tickerStart = true;
            }
          }
          if (json.containsKey("setConfig") && root["setConfig"]) {
              conf.setConfig(root);
          }
      }
    }

    // smallest configured reporting interval of all active data services,
    // used to pick the BME680 sample rate
    int minDataFrequency() {
      int minFreq = INT_MAX;

      if (serveData.mqtt.active && serveData.mqtt.frequency > 0) {
        minFreq = min(minFreq, serveData.mqtt.frequency);
      }
      if (serveData.get.active && serveData.get.frequency > 0) {
        minFreq = min(minFreq, serveData.get.frequency);
      }
      if (serveData.post.active && serveData.post.frequency > 0) {
        minFreq = min(minFreq, serveData.post.frequency);
      }
      // HA reports on the MQTT interval
      if (serveData.ha.active && serveData.mqtt.frequency > 0) {
        minFreq = min(minFreq, serveData.mqtt.frequency);
      }

      // If no services are active or all frequencies are 0, default to 60s
      if (minFreq == INT_MAX || minFreq == 0) {
        minFreq = 60;
      }

      D_println("Minimum data frequency: " + String(minFreq) + "s");
      return minFreq;
    }

    bool anyServeModeActive() {
      return serveData.get.active || serveData.post.active ||
             serveData.mqtt.active || serveData.ha.active;
    }

    // data logging needs both the RTC (timestamps) and the EEPROM (storage)
    bool dataLogAvailable() {
      return peripherals.eeprom && peripherals.ds3231;
    }

    // appends the current measurements with a timestamp to the EEPROM
    // day file; at most one entry per minute
    void logSensorData() {
      if (!dataLogAvailable() || device.configMode) {
        return;
      }
      time.update();
      if (!time.isTimeSet()) {
        return; // clock was never set, timestamps would be useless
      }
      const String stamp = time.timestamp();
      if (stamp == m_lastLogStamp) {
        return;
      }

      // measured values only; derived ones can be recalculated
      static const char *loggedKeys[] = {"temp", "humi", "temp2", "humi2",
                                         "qfe", "alt", "lux", "adc",
                                         "iaq", "eco2", "bvoc", "air"};
      String line = stamp;
      for (const char *key : loggedKeys) {
        if (sensorData.containsKey(key)) {
          line += String(" ") + key + "=" + sensorData[key].as<String>();
        }
      }
      line += "\n";

      const String fileName = String(time.getMonthDay()) + ".txt";
      if (eeprom.appendLine(fileName, line, time.getMonthDay())) {
        m_lastLogStamp = stamp;
        D_print(F("Data log: "));
        D_print(line);
      }
    }

    bool sleepEnabled()
    {
      return device.sleepMode || device.lightSleepMode;
    }
    void shouldSensorDataBeGarbageCollected(bool value)
    {
      m_sensorDataGarbageCollect = value;
    }

    bool tickerStop{false};
    bool tickerStart{false};

  private:
    DHTesp & m_dht;
    UUID m_uuid;
    bool m_sensorDataGarbageCollect{false};
    String m_lastLogStamp;
    void setDeviceKey(String key) {
      device.key = key;
      sensorData["key"] = key;
    }
    String generateDeviceKey() {
      m_uuid.seed(ESP.getChipId());
      m_uuid.generate();
      return String(m_uuid.toCharArray());
    }
    float calibrateValue(const String & key, float value) {
      if (calibration.active) {
        if (key == "temp" || key == "temp2")
          value += calibration.temp;
        else if (key == "humi" || key == "humi2")
          value += calibration.humi;
        else if (key == "qfe")
          value += calibration.qfe;
      }
      return value;
    }
    void sensorDataGarbageCollect()
    {
       if(!device.sleepMode)
       {
          m_sensorDataGarbageCollect = false;
          sensorData.garbageCollect();
       }
    }
};
