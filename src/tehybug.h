#pragma once
#include <climits>
#include <ArduinoJson.h>
#include "DHTesp.h"
#include "debug.h"
#include "common_functions.h"
#include "data_types.h"
#include "mode_logic.h"
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
    // Declaration order is construction order, and each of these binds a
    // reference to the ones above it: conf takes pixel, time takes conf,
    // eeprom takes time. pixel used to be declared last, so conf bound a
    // reference to storage that had not been constructed yet.
    TeHyBugPixel pixel;
    TeHyBugConfig conf;
    RtcTime time;
    TeHyBugEeprom eeprom;

    // initialiser order matches the declarations above (m_dht is declared last)
    TeHyBug(DHTesp & dht)
      : sensorData(1024),
        conf(calibration, sensor, peripherals, device, serveData, scenarios, pixel),
        time(conf),
        eeprom(time),
        m_dht(dht) {
    }

    // Expands %key% placeholders from the current sensor readings.
    // The text scan lives in common_functions.h so it can be unit-tested; it
    // replaced a loop over every key in sensorData that ran String::replace
    // ~25 times per URL, payload and log line.
    String replacePlaceholders(const String & text, bool dropUnknown = false) {
      const JsonObject root = sensorData.as<JsonObject>();
      return expandPlaceholders(text, root, dropUnknown);
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
      }
      // Always publish it into sensorData: when the key came from the stored
      // config this was skipped, so the "key" placeholder resolved to nothing
      // in every pushed payload.
      setDeviceKey(key);
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
      const int minFreq = mode_logic::minDataFrequency(serveData);
      D_println("Minimum data frequency: " + String(minFreq) + "s");
      return minFreq;
    }

    bool anyServeModeActive() {
      return mode_logic::anyServeModeActive(serveData);
    }

    bool anyScenarioActive() {
      return mode_logic::anyScenarioActive(scenarios);
    }

    // How long the device sleeps between wakes: the shortest configured
    // interval, so adding a second service cannot starve the first. Note the
    // EEPROM log counts — a 10 s log interval means 10 s wakes even if every
    // network service reports hourly. 0 when nothing is configured.
    int wakeIntervalSeconds() {
      return mode_logic::wakeInterval(serveData);
    }

    /* Operating mode — resolved in one place (mode_logic.h) from the stored
       config and the hardware present, instead of re-deriving it from boolean
       combinations at every call site. */

    mode_logic::DeviceMode mode() {
      return mode_logic::currentMode(device, peripherals);
    }
    const char *modeName() {
      return mode_logic::modeName(mode());
    }
    bool inConfigMode()     { return mode() == mode_logic::DeviceMode::Config; }
    bool inOfflineMode()    { return mode() == mode_logic::DeviceMode::Offline; }
    bool inDeepSleepMode()  { return mode() == mode_logic::DeviceMode::DeepSleep; }
    bool inLightSleepMode() { return mode() == mode_logic::DeviceMode::LightSleep; }
    bool inLiveMode()       { return mode() == mode_logic::DeviceMode::Live; }
    // either sleeping mode
    bool inSleepMode()      { return mode_logic::isSleeping(mode()); }

    // Config mode is the one mode that is switched directly (MODE button,
    // remote control, first start); the others follow from the configuration.
    void setConfigMode(bool on) {
      device.configMode = on;
    }

    // EEPROM-only mode: no WiFi, measure + log + deep-sleep. Needs the
    // EEPROM peripheral present (the generic 1MB build has no EEPROM driver).
    bool offlineEnabled() {
      return mode_logic::offlineEnabled(device, peripherals);
    }

    // data logging needs both the RTC (timestamps) and the EEPROM (storage)
    bool dataLogAvailable() {
      return mode_logic::dataLogAvailable(peripherals);
    }

    // Wipe the data log when the configured period (hourly vs monthly) differs
    // from what the stored data was written with, so switching period starts
    // clean instead of leaving the other layout's files behind. The period is
    // recorded in a reserved index key. A device with no marker yet (fresh, or
    // upgraded from before this option existed) just adopts the current period
    // without wiping, so existing logs survive a firmware update. Call once
    // after the EEPROM is mounted.
    void syncDataLogMode() {
      if (!eeprom.mounted()) {
        return;
      }
      // hour-of-day slots wrap at 24, day-of-month slots at 31; the log needs
      // this to pick the right "oldest" file when it recycles one
      eeprom.setSlotWrap(serveData.eeprom.hourly ? 24 : 31);
      const String want = serveData.eeprom.hourly ? "hour" : "month";
      const String have = eeprom.fileDate(DATALOG_MODE_KEY);
      if (have.length() == 0) {
        eeprom.setFileDate(DATALOG_MODE_KEY, want);  // first run: adopt, keep data
      } else if (have != want) {
        D_println(F("Data log period changed, wiping EEPROM"));
        eeprom.format();                             // clear every slot
        eeprom.setFileDate(DATALOG_MODE_KEY, want);
      }
    }

    // appends the current measurements with a timestamp to the EEPROM
    // day file; at most one entry per minute (the timestamp resolution)
    void logSensorData() {
      if (!serveData.eeprom.active || !dataLogAvailable() || device.configMode) {
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

      // Slots are named by day-of-month (month mode: 31 files, a rolling month)
      // or by hour-of-day (hourly mode: 24 files, a rolling 24 h). Either way a
      // slot is reused when its day/hour comes round again, so it could still
      // hold the previous period's data. Each slot records the period it
      // currently holds in the index ("YYYY-MM-DD" or "YYYY-MM-DD HH"); the
      // first time we log into a given period, if the slot's label differs the
      // file is stale — clear it and record the new label. m_lastLogSlot caches
      // this within a session so the index is not re-read every write.
      uint8_t slot;
      String label;
      if (serveData.eeprom.hourly) {
        slot = time.getHours();                                  // 0-23
        label = time.dateString() + " " + IntFormat(slot);       // "YYYY-MM-DD HH"
      } else {
        slot = time.getMonthDay();                               // 1-31
        label = time.dateString();                               // "YYYY-MM-DD"
      }
      const String fileName = String(slot) + ".txt";
      if (m_lastLogSlot != label) {
        if (eeprom.fileDate(slot) != label) {
          eeprom.resetDayFile(fileName, slot);
          eeprom.setFileDate(slot, label);
        }
        m_lastLogSlot = label;
      }

      // Compact format to fit more entries in the small EEPROM slots: the
      // date is implied by the per-day file name, so only "HH:MM" is stored,
      // and each default value is written as "<value><code>" (e.g.
      // "22.6t 48.3h 1013.2p") reusing the same one-letter field codes as
      // the cloud GET URL. A custom placeholder template (e.g. "%temp%
      // %humi%") instead logs exactly what the user wrote.
      String line = time.timeOfDay() + " ";
      if (serveData.eeprom.message.length() > 0) {
        // Drop placeholders with no reading behind them rather than writing
        // them out: a template naming a sensor this device does not have (a
        // "%qfe%" with no pressure sensor) otherwise spends those bytes on
        // every single entry, in the one place where space is measured in
        // hundreds of bytes.
        line += replacePlaceholders(serveData.eeprom.message, true);
      } else {
        line += compactLogLine(sensorData.as<JsonObject>());
      }
      // A dropped placeholder can leave the separator that preceded it
      // stranded at the end of the line.
      while (line.length() > 0 && line.charAt(line.length() - 1) == ' ') {
        line = line.substring(0, line.length() - 1);
      }
      line += "\n";

      if (eeprom.appendLine(fileName, line, slot)) {
        m_lastLogStamp = stamp;
        D_print(F("Data log: "));
        D_print(line);
      }
    }

    bool sleepEnabled()
    {
      return mode_logic::sleepEnabled(device);
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
    String m_lastLogSlot;
    // Reserved index key (not a valid day-of-month 1-31 or hour-of-day 0-23)
    // that records which period the logged data was written with.
    static constexpr uint8_t DATALOG_MODE_KEY = 200;
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
