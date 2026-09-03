#pragma once
#include <Arduino.h>

struct RemoteControl {
  bool active{false};
  String url{};
};
struct Device {
  String key;
  bool configMode{true};
  bool sleepMode{false};
  bool lightSleepMode{false};
  // EEPROM-only mode: the device never brings up WiFi, it just measures,
  // appends to the offline log and deep-sleeps. Hold the MODE button on
  // boot to re-enter config mode (WiFi on) and read the log.
  bool offlineMode{false};
  RemoteControl remoteControl{};
};
struct Sensor {
  bool bmx{false};
  bool bme680{false};
  bool max44009{false};
  bool aht20{false};
  bool dht{false};
  bool dht_2{false};
  bool am2320{false};
  bool ds18b20{false};
  bool ds18b20_2{false};
  bool adc{false};
  bool sgp30{false};
} __attribute__((packed));
struct Peripherals {
  bool eeprom{false};
  bool ds3231{false};
} __attribute__((packed));
struct Calibration {
  bool active{false};
  float temp{0};
  float humi{0};
  float qfe{0};
};
struct Scenario {
  bool active{false};
  String type{};
  String url{};
  String data{};
  String condition{};
  float value{};
  String message{};
};
struct Scenarios {
  static constexpr uint8_t count{3};
  Scenario items[count]{};
};
struct HttpGetDataServ {
  String url;
  bool active{false};
  int frequency{900};
};
struct HttpPostDataServ {
  String url;
  bool active{false};
  int frequency{900};
  String message;
};
struct MqttDataServ {
  bool active{false};
  bool retained{false};
  String user;
  String password;
  String server{"0.0.0.0"};
  String topic{"/tehybug"};
  String message;
  int port{1883};
  int frequency{900};
  uint8_t retryCounter{0};
  uint8_t maxRetries{10};
};
struct HaDataServ {
  bool active{false};
};
// Offline data log on the I2C EEPROM. `message` is a placeholder template
// (e.g. "%temp% %humi%") expanded per entry; empty means log the default
// measured-value set. `frequency` is the seconds between log writes and,
// in offline mode, the deep-sleep interval.
//
// `hourly` chooses the slot granularity:
//   false (default) - one file per day of month (31 files): a rolling month.
//   true            - one file per hour of day (24 files): a rolling 24 hours
//                     at finer detail. Each slot is reused when its hour/day
//                     comes round again.
struct EepromDataServ {
  bool active{false};
  int frequency{60};
  String message;
  bool hourly{false};
};
struct DataServ {
  HttpGetDataServ get{};
  HttpPostDataServ post{};
  MqttDataServ mqtt{};
  HaDataServ ha{};
  EepromDataServ eeprom{};
};

/* Display board (TeHyBug Display Weatherstation) ----------------------------
 *
 * The types are unconditional so the pure display logic stays host-testable;
 * the display build gate (TEHYBUG_DISPLAY) only decides whether the config
 * carries these keys and whether the hardware modules are compiled in.
 */

// One weekday-scheduled alarm. `time` is "HH:MM"; `weekdays` is a CSV of 7
// 0/1 flags Monday..Sunday — the format the original display firmware stored,
// kept so an upgraded device's config keeps its alarms.
struct AlarmConf {
  bool active{false};
  String time{};
  String message{};
  String weekdays{"0,0,0,0,0,0,0"};
};
struct Alarms {
  static constexpr uint8_t count{3};
  AlarmConf items[count]{};
};

// OLED configuration. line1..3 are %placeholder% templates (the clock page
// shows line1+line2 in its footer, the sensor page all three). The clock_*
// keys keep the original firmware's names; night mode (clock_sleep) blanks
// the panel between two "HH:MM" times.
struct DisplayConf {
  String line1{"%temp% °C"};
  String line2{"%humi% %RH"};
  String line3{"%qfe% hPa"};
  bool clock12h{false};
  bool showIp{true};
  bool nightMode{false};
  String nightStart{"22:00"};
  String nightEnd{"07:00"};
};
