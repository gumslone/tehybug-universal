// Does the configuration still fit the document pool it is built in?
//
// ArduinoJson does not fail loudly when a DynamicJsonDocument runs out of
// pool: it drops members. For the config that means settings silently
// disappearing on save or on the UI dump, which is the kind of bug nobody
// reports as "the JSON was too big" — they report that the device forgot the
// MQTT password. This test builds a deliberately worst-case config with the
// real library and asserts it still fits, so adding keys (as the display
// build did) cannot quietly eat the last of the headroom.
//
// The field inventory below mirrors TeHyBugConfig::buildConfig(full=true).
// Adding a config key there means adding it here too — that is the point.
#include "test_framework.h"
#include <ArduinoJson.h>
#include <string>

// keep in sync with src/configuration.h
static constexpr size_t CONFIG_DOC_SIZE_DEFAULT = 4608;
static constexpr size_t CONFIG_DOC_SIZE_DISPLAY = 5632;

static std::string filler(size_t n, char c = 'x') { return std::string(n, c); }

// Builds the full config dump with generous values for every free-text
// field: long cloud URLs, a big MQTT payload template, three fully populated
// scenarios. `display` adds the keys only the display build carries.
static size_t buildWorstCase(size_t poolSize, bool display, bool &overflowed) {
  DynamicJsonDocument json(poolSize);

  json[std::string("key")] = filler(36); // UUID

  // MQTT
  json[std::string("mqttActive")] = true;
  json[std::string("mqttRetained")] = true;
  json[std::string("mqttUser")] = filler(32);
  json[std::string("mqttPassword")] = filler(63);
  json[std::string("mqttServer")] = filler(40);
  json[std::string("mqttMasterTopic")] = filler(60);
  json[std::string("mqttMessage")] = filler(250); // full placeholder payload
  json[std::string("mqttPort")] = 1883;
  json[std::string("mqttFrequency")] = 900;

  json[std::string("haActive")] = true;

  // EEPROM data log
  json[std::string("eepromLogActive")] = true;
  json[std::string("eepromLogFrequency")] = 60;
  json[std::string("eepromLogMessage")] = filler(80);
  json[std::string("eepromLogHourly")] = true;
  json[std::string("offlineModeActive")] = true;

  // HTTP
  json[std::string("httpGetURL")] = filler(200);
  json[std::string("httpsFingerprint")] = filler(240); // three "host AB:CD:..." entries
  json[std::string("ntpActive")] = true;
  json[std::string("ntpServer")] = filler(40);
  json[std::string("timezone")] = filler(40); // a POSIX TZ string with DST rules
  json[std::string("httpGetActive")] = true;
  json[std::string("httpGetFrequency")] = 900;
  json[std::string("httpPostURL")] = filler(200);
  json[std::string("httpPostActive")] = true;
  json[std::string("httpPostFrequency")] = 900;
  json[std::string("httpPostJson")] = filler(250);

  // calibration
  json[std::string("calibrationActive")] = true;
  json[std::string("calibrationTemp")] = 1.5;
  json[std::string("calibrationHumi")] = 1.5;
  json[std::string("calibrationQfe")] = 1.5;

  // modes
  json[std::string("configModeActive")] = true;
  json[std::string("sleepModeActive")] = true;
  json[std::string("lightSleepModeActive")] = true;

  // sensors
  json[std::string("dht_sensor")] = true;
  json[std::string("second_dht_sensor")] = true;
  json[std::string("ds18b20_sensor")] = true;
  json[std::string("second_ds18b20_sensor")] = true;
  json[std::string("adc_sensor")] = true;

  // three scenarios, all populated
  for (int i = 1; i <= 3; i++) {
    const std::string p = "sc" + std::to_string(i) + "_";
    json[p + "active"] = true;
    json[p + "type"] = std::string("io13_1");
    json[p + "url"] = filler(120);
    json[p + "data"] = filler(150);
    json[p + "condition"] = std::string("gt");
    json[p + "value"] = 42.5;
    json[p + "message"] = filler(60);
  }

  // remote control
  json[std::string("rc_active")] = true;
  json[std::string("rc_url")] = filler(200);

  json[std::string("board")] = std::string("universal");

  if (display) {
    json[std::string("line1")] = filler(40);
    json[std::string("line2")] = filler(40);
    json[std::string("line3")] = filler(40);
    json[std::string("clock_12h")] = true;
    json[std::string("clock_show_ip")] = true;
    json[std::string("clock_sleep")] = true;
    json[std::string("clock_sleep_start")] = std::string("22:00");
    json[std::string("clock_sleep_finish")] = std::string("07:00");
    for (int i = 1; i <= 3; i++) {
      const std::string p = "alarm" + std::to_string(i);
      json[p + "Active"] = true;
      json[p + "Time"] = std::string("07:30");
      json[p + "Message"] = filler(60);
      json[p + "Weekdays"] = std::string("1,1,1,1,1,1,1");
    }
  }

  overflowed = json.overflowed();
  return json.memoryUsage();
}

static void test_fits() {
  bool overflowed = false;

  CASE("the universal config fits its pool");
  size_t used = buildWorstCase(CONFIG_DOC_SIZE_DEFAULT, false, overflowed);
  std::printf("  universal: %zu / %zu bytes used\n", used,
              CONFIG_DOC_SIZE_DEFAULT);
  CHECK(!overflowed);

  CASE("the display config fits its (larger) pool");
  used = buildWorstCase(CONFIG_DOC_SIZE_DISPLAY, true, overflowed);
  std::printf("  display:   %zu / %zu bytes used\n", used,
              CONFIG_DOC_SIZE_DISPLAY);
  CHECK(!overflowed);

  CASE("the pool sizes are not arbitrary: 3072 really was too small");
  // Both builds overflowed the pool this replaced - the universal one too,
  // which is why the size went up for every board and not just the display.
  // If this ever stops holding, the pools can come back down.
  buildWorstCase(3072, false, overflowed);
  CHECK(overflowed);
  buildWorstCase(3072, true, overflowed);
  CHECK(overflowed);
}

int main() {
  std::printf("Running config size tests...\n");
  test_fits();
  return SUMMARY();
}
