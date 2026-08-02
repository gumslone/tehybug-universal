// Native host tests for the boot/serve decision logic that setup() and loop()
// hinge on (mode_logic.h).
#include "test_framework.h"
#include "../src/mode_logic.h"

using namespace mode_logic;

static void test_sleep_enabled() {
  CASE("sleepEnabled");
  Device d;
  CHECK(!sleepEnabled(d)); // default: awake
  d.sleepMode = true;
  CHECK(sleepEnabled(d)); // deep sleep
  d.sleepMode = false;
  d.lightSleepMode = true;
  CHECK(sleepEnabled(d)); // light sleep
}

static void test_offline_enabled() {
  CASE("offlineEnabled needs the EEPROM present");
  Device d;
  Peripherals p;
  d.offlineMode = true;
  CHECK(!offlineEnabled(d, p)); // configured but no EEPROM -> falls back to WiFi
  p.eeprom = true;
  CHECK(offlineEnabled(d, p)); // configured and EEPROM present
  d.offlineMode = false;
  CHECK(!offlineEnabled(d, p)); // EEPROM present but not requested
}

static void test_any_serve_mode_active() {
  CASE("anyServeModeActive");
  DataServ s;
  CHECK(!anyServeModeActive(s)); // nothing selected -> forces config mode
  s.get.active = true;
  CHECK(anyServeModeActive(s));
  s.get.active = false;
  s.eeprom.active = true; // offline logging counts as a serve mode
  CHECK(anyServeModeActive(s));
  s.eeprom.active = false;
  s.ha.active = true;
  CHECK(anyServeModeActive(s));
}

static void test_data_log_available() {
  CASE("dataLogAvailable needs RTC and EEPROM");
  Peripherals p;
  CHECK(!dataLogAvailable(p));
  p.eeprom = true;
  CHECK(!dataLogAvailable(p)); // EEPROM but no clock
  p.ds3231 = true;
  CHECK(dataLogAvailable(p));
}

static void test_min_data_frequency() {
  CASE("minDataFrequency picks the smallest active interval");
  DataServ s;
  CHECK(minDataFrequency(s) == 60); // nothing active -> default 60s

  s.mqtt.active = true;
  s.mqtt.frequency = 900;
  CHECK(minDataFrequency(s) == 900);

  s.get.active = true;
  s.get.frequency = 300;
  CHECK(minDataFrequency(s) == 300); // smallest of the active ones

  s.post.active = true;
  s.post.frequency = 120;
  CHECK(minDataFrequency(s) == 120);

  // HA reports on the MQTT interval, not its own
  DataServ h;
  h.ha.active = true;
  h.mqtt.frequency = 600;
  CHECK(minDataFrequency(h) == 600);

  // a zero/invalid frequency is ignored, falling back to the default
  DataServ z;
  z.get.active = true;
  z.get.frequency = 0;
  CHECK(minDataFrequency(z) == 60);
}

static void test_any_scenario_active() {
  CASE("anyScenarioActive");
  Scenarios none;
  CHECK(!anyScenarioActive(none)); // default: nothing configured

  // each slot on its own must be detected, so live mode starts the ticker
  for (uint8_t i = 0; i < Scenarios::count; i++) {
    Scenarios sc;
    sc.items[i].active = true;
    CHECK(anyScenarioActive(sc));
  }

  Scenarios all;
  for (uint8_t i = 0; i < Scenarios::count; i++) {
    all.items[i].active = true;
  }
  CHECK(anyScenarioActive(all));
}

static void test_current_mode() {
  CASE("currentMode precedence");
  Device d;
  Peripherals p;

  // nothing configured: config mode is the default state
  CHECK(currentMode(d, p) == DeviceMode::Config);

  d.configMode = false;
  CHECK(currentMode(d, p) == DeviceMode::Live);

  d.sleepMode = true;
  CHECK(currentMode(d, p) == DeviceMode::DeepSleep);
  CHECK(isSleeping(currentMode(d, p)));
  d.sleepMode = false;
  d.lightSleepMode = true;
  // light sleep is its own mode: execution resumes in loop() and the WiFi
  // association has to be rebuilt, unlike deep sleep which resets the chip
  CHECK(currentMode(d, p) == DeviceMode::LightSleep);
  CHECK(isSleeping(currentMode(d, p)));
  // with both set, deep sleep wins (matching startSleep's order)
  d.sleepMode = true;
  CHECK(currentMode(d, p) == DeviceMode::DeepSleep);
  d.sleepMode = false;
  d.lightSleepMode = false;
  CHECK(!isSleeping(currentMode(d, p)));

  // offline only engages when the EEPROM is actually present
  d.offlineMode = true;
  CHECK(currentMode(d, p) == DeviceMode::Live); // no EEPROM -> not offline
  p.eeprom = true;
  CHECK(currentMode(d, p) == DeviceMode::Offline);

  // offline outranks sleep ...
  d.sleepMode = true;
  CHECK(currentMode(d, p) == DeviceMode::Offline);

  // ... and config outranks everything, so the MODE button always gets back in
  d.configMode = true;
  CHECK(currentMode(d, p) == DeviceMode::Config);

  CHECK_EQ_STR(modeName(DeviceMode::Config), "config");
  CHECK_EQ_STR(modeName(DeviceMode::Offline), "offline");
  CHECK_EQ_STR(modeName(DeviceMode::DeepSleep), "deep-sleep");
  CHECK_EQ_STR(modeName(DeviceMode::LightSleep), "light-sleep");
  CHECK_EQ_STR(modeName(DeviceMode::Live), "live");
}

static void test_wake_interval() {
  CASE("wakeInterval picks the shortest configured interval");
  DataServ s;
  CHECK(wakeInterval(s) == 0); // nothing configured -> no scheduled sleep

  // a network service alone: its own interval
  s.mqtt.active = true;
  s.mqtt.frequency = 3600;
  CHECK(wakeInterval(s) == 3600);

  // the EEPROM log alone: the log interval
  DataServ e;
  e.eeprom.active = true;
  e.eeprom.frequency = 900;
  CHECK(wakeInterval(e) == 900);

  // The surprise this exists to document: a short log interval decides how
  // often the device wakes, however rarely the network services report. An
  // hourly MQTT report with a 10 s log is 10 s wakes, not hourly ones — on
  // battery that is the difference between months and days.
  DataServ both;
  both.ha.active = true;
  both.mqtt.frequency = 3600;
  both.eeprom.active = true;
  both.eeprom.frequency = 10;
  CHECK(wakeInterval(both) == 10);

  // and the other way round, the log does not stretch a shorter report
  both.eeprom.frequency = 7200;
  CHECK(wakeInterval(both) == 3600);

  // HA reports on the MQTT interval, so it counts as a network service
  DataServ ha;
  ha.ha.active = true;
  ha.mqtt.frequency = 1800;
  CHECK(wakeInterval(ha) == 1800);

  // an inactive log is ignored even with a tiny frequency
  DataServ off;
  off.get.active = true;
  off.get.frequency = 600;
  off.eeprom.frequency = 5; // not active
  CHECK(wakeInterval(off) == 600);
}

static void test_scenario_condition() {
  CASE("scenarioConditionMet");
  CHECK(scenarioConditionMet("lt", 1.0f, 2.0f));
  CHECK(!scenarioConditionMet("lt", 2.0f, 2.0f));
  CHECK(scenarioConditionMet("gt", 3.0f, 2.0f));
  CHECK(!scenarioConditionMet("gt", 2.0f, 2.0f));
  CHECK(scenarioConditionMet("eq", 2.0f, 2.0f));
  CHECK(!scenarioConditionMet("eq", 2.1f, 2.0f));
  // an unknown operator matches nothing - it must never fire an action
  CHECK(!scenarioConditionMet("", 1.0f, 2.0f));
  CHECK(!scenarioConditionMet("ne", 1.0f, 2.0f));
}

int main() {
  std::printf("Running mode_logic tests...\n");
  test_scenario_condition();
  test_wake_interval();
  test_current_mode();
  test_sleep_enabled();
  test_offline_enabled();
  test_any_serve_mode_active();
  test_data_log_available();
  test_min_data_frequency();
  test_any_scenario_active();
  return SUMMARY();
}
