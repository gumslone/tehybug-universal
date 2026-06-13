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
struct EepromDataServ {
  bool active{false};
  int frequency{60};
  String message;
};
struct DataServ {
  HttpGetDataServ get{};
  HttpPostDataServ post{};
  MqttDataServ mqtt{};
  HaDataServ ha{};
  EepromDataServ eeprom{};
};
