// Native host tests for the pure helpers in common_functions.h.
#include "test_framework.h"
#include "../src/common_functions.h"

static void test_int_format() {
  CASE("IntFormat zero-pads below 10");
  CHECK_EQ_STR(IntFormat(0).c_str(), "00");
  CHECK_EQ_STR(IntFormat(5).c_str(), "05");
  CHECK_EQ_STR(IntFormat(9).c_str(), "09");
  CHECK_EQ_STR(IntFormat(10).c_str(), "10");
  CHECK_EQ_STR(IntFormat(31).c_str(), "31");
}

static void test_rssi_quality() {
  CASE("GetRSSIasQuality maps dBm to percent");
  CHECK(GetRSSIasQuality(-100) == 0);
  CHECK(GetRSSIasQuality(-120) == 0);   // clamped low
  CHECK(GetRSSIasQuality(-50) == 100);
  CHECK(GetRSSIasQuality(-10) == 100);  // clamped high
  CHECK(GetRSSIasQuality(-75) == 50);   // 2 * (-75 + 100)
}

static void test_temp_to_imperial() {
  CASE("temp2Imp converts C to F");
  CHECK(temp2Imp(0.0f) == 32.0f);
  CHECK(temp2Imp(100.0f) == 212.0f);
  CHECK(temp2Imp(37.0f) == 98.6f);
}

static void test_io_scenario_parsing() {
  CASE("io scenario type parsing");
  CHECK(isIoScenario("io13_1"));
  CHECK(isIoScenario("io13_0"));
  CHECK(!isIoScenario("get"));
  CHECK(!isIoScenario("post"));
  // "io13_1" -> pin 13, level 1 ; "io13_0" -> pin 13, level 0
  CHECK(ioScenarioPin("io13_1") == 13);
  CHECK(ioScenarioPin("io13_0") == 13);
  CHECK(ioScenarioLevel("io13_1") == 1);
  CHECK(ioScenarioLevel("io13_0") == 0);
}

static void test_key_to_unit() {
  CASE("key2unit mapping");
  CHECK_EQ_STR(key2unit("temp").c_str(), "\xC2\xB0""C"); // °C
  CHECK_EQ_STR(key2unit("temp_imp").c_str(), "\xC2\xB0""F"); // °F
  CHECK_EQ_STR(key2unit("humi").c_str(), "%RH");
  CHECK_EQ_STR(key2unit("qfe").c_str(), "hPa");
  CHECK_EQ_STR(key2unit("lux").c_str(), "Lux");
  CHECK_EQ_STR(key2unit("nope").c_str(), "");
}

static void test_key_to_name() {
  CASE("key2name mapping");
  CHECK_EQ_STR(key2name("temp").c_str(), "Temperature");
  CHECK_EQ_STR(key2name("humi").c_str(), "Humidity");
  CHECK_EQ_STR(key2name("qfe").c_str(), "Atmospheric pressure");
  CHECK_EQ_STR(key2name("iaq").c_str(), "Indoor air quality");
  CHECK_EQ_STR(key2name("nope").c_str(), "");
}

static void test_comfort_state_name() {
  CASE("cf2name mapping");
  CHECK_EQ_STR(cf2name(Comfort_OK).c_str(), "OK");
  CHECK_EQ_STR(cf2name(Comfort_TooHot).c_str(), "Too hot");
  CHECK_EQ_STR(cf2name(Comfort_ColdAndDry).c_str(), "Cold and dry");
  CHECK_EQ_STR(cf2name(999).c_str(), "Unknown");
}

static void test_second_sensor_metadata() {
  CASE("second-sensor keys inherit unit/name/icon");
  // Home Assistant received empty names and units for every second-sensor and
  // derived value before these fell back to the base key.
  CHECK_EQ_STR(key2unit("humi2").c_str(), "%RH");
  CHECK_EQ_STR(key2unit("hi2").c_str(), "\xC2\xB0""C");
  CHECK_EQ_STR(key2unit("dew2").c_str(), "\xC2\xB0""C");
  CHECK_EQ_STR(key2unit("ah2").c_str(), "g/m\xC2\xB3");
  CHECK_EQ_STR(key2unit("cr2").c_str(), "%");
  CHECK_EQ_STR(key2unit("hi_imp2").c_str(), "\xC2\xB0""F");

  CHECK_EQ_STR(key2name("humi2").c_str(), "Humidity 2");
  CHECK_EQ_STR(key2name("dew2").c_str(), "Dew point 2");
  CHECK_EQ_STR(key2name("cs2").c_str(), "Comfort status 2");
  CHECK_EQ_STR(key2name("temp2").c_str(), "Temperature 2");
  CHECK(key2icon("humi2") == key2icon("humi"));
  CHECK(key2icon("dew2") == key2icon("dew"));

  // the base keys must be untouched
  CHECK_EQ_STR(key2name("humi").c_str(), "Humidity");
  CHECK_EQ_STR(key2unit("temp").c_str(), "\xC2\xB0""C");

  // eco2 is a sensor of its own, not a second-sensor variant of "eco"
  CHECK_EQ_STR(key2name("eco2").c_str(), "CO2 equivalent");
  CHECK_EQ_STR(key2unit("eco2").c_str(), "ppm");
  CHECK_EQ_STR(baseKey("eco2").c_str(), "eco2");
  CHECK_EQ_STR(baseKey("humi2").c_str(), "humi");
  CHECK_EQ_STR(baseKey("humi").c_str(), "humi");

  // bvoc had neither a unit nor an icon
  CHECK_EQ_STR(key2unit("bvoc").c_str(), "ppm");
  CHECK(key2icon("bvoc") != String("mdi:help"));

  // an unknown key still yields nothing rather than a bogus " 2"
  CHECK_EQ_STR(key2name("nope2").c_str(), "");
}

static void test_expand_placeholders() {
  CASE("expandPlaceholders");
  DynamicJsonDocument doc(256);
  doc["temp"] = "22.6";
  doc["humi"] = "48.3";
  const JsonObject v = doc.as<JsonObject>();

  // the common cases: a URL and a message template
  CHECK_EQ_STR(expandPlaceholders("t=%temp%&h=%humi%", v).c_str(),
               "t=22.6&h=48.3");
  CHECK_EQ_STR(expandPlaceholders("%temp%", v).c_str(), "22.6");
  CHECK_EQ_STR(expandPlaceholders("%temp% %humi%", v).c_str(), "22.6 48.3");

  // text without placeholders is returned unchanged
  CHECK_EQ_STR(expandPlaceholders("no placeholders", v).c_str(),
               "no placeholders");
  CHECK_EQ_STR(expandPlaceholders("", v).c_str(), "");

  // an unknown key is left exactly as written, not blanked
  CHECK_EQ_STR(expandPlaceholders("a %nope% b", v).c_str(), "a %nope% b");
  CHECK_EQ_STR(expandPlaceholders("%temp% %nope%", v).c_str(), "22.6 %nope%");

  // a lone or unterminated % must not eat the rest of the string
  // dropUnknown: the data log asks for unresolved placeholders to be removed
  // instead of written out, so a template naming a sensor the device does not
  // have does not spend those bytes on every entry.
  CHECK_EQ_STR(expandPlaceholders("a %nope% b", v, true).c_str(), "a  b");
  CHECK_EQ_STR(expandPlaceholders("%temp% %nope%", v, true).c_str(), "22.6 ");
  CHECK_EQ_STR(expandPlaceholders("%nope%", v, true).c_str(), "");
  // the real case: "%temp% %humi% %qfe%" on a device with no pressure sensor
  CHECK_EQ_STR(expandPlaceholders("%temp% %humi% %qfe%", v, true).c_str(),
               "22.6 48.3 ");
  // known keys are unaffected by the flag
  CHECK_EQ_STR(expandPlaceholders("%temp% %humi%", v, true).c_str(), "22.6 48.3");
  // and the default still leaves them verbatim, which is what a URL wants
  CHECK_EQ_STR(expandPlaceholders("a %nope% b", v).c_str(), "a %nope% b");

  CHECK_EQ_STR(expandPlaceholders("100% humidity", v).c_str(), "100% humidity");
  CHECK_EQ_STR(expandPlaceholders("%temp% is 100%", v).c_str(), "22.6 is 100%");
  CHECK_EQ_STR(expandPlaceholders("%", v).c_str(), "%");
  CHECK_EQ_STR(expandPlaceholders("%%", v).c_str(), "%%"); // empty key, unknown

  // adjacent placeholders and surrounding text
  CHECK_EQ_STR(expandPlaceholders("[%temp%%humi%]", v).c_str(), "[22.648.3]");
}

int main() {
  std::printf("Running common_functions tests...\n");
  test_int_format();
  test_rssi_quality();
  test_temp_to_imperial();
  test_io_scenario_parsing();
  test_key_to_unit();
  test_key_to_name();
  test_comfort_state_name();
  test_second_sensor_metadata();
  test_expand_placeholders();
  return SUMMARY();
}
