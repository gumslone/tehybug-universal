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

int main() {
  std::printf("Running mode_logic tests...\n");
  test_sleep_enabled();
  test_offline_enabled();
  test_any_serve_mode_active();
  test_data_log_available();
  test_min_data_frequency();
  return SUMMARY();
}
