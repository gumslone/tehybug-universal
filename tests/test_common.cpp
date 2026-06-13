// Native host tests for the pure helpers in common_functions.h.
#include "test_framework.h"
#include "../common_functions.h"

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

int main() {
  std::printf("Running common_functions tests...\n");
  test_int_format();
  test_rssi_quality();
  test_temp_to_imperial();
  test_io_scenario_parsing();
  test_key_to_unit();
  test_key_to_name();
  test_comfort_state_name();
  return SUMMARY();
}
