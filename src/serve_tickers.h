#pragma once
// Live-mode scheduling: one ticker slot per active data service, plus the
// scenario evaluator. Sleep modes never use these — their cycle is driven by
// loop() and serve_data() directly.
#include "globals.h"
#include "data_service.h"
#include "mqtt_service.h"
#include "sensors.h"
#include "debug.h"

void addServeTicker(uint8_t slot, int frequencySeconds, std::function<void()> send) {
  const bool added = ticker.add(
    slot, (uint32_t)frequencySeconds * 1000,
  [send](void *) {
    read_sensors();
    yield();
    send();
  },
  nullptr, true);
  // A rejected slot means that service silently never fires again, which is
  // hard to spot in the field — say so instead of failing quietly.
  if (!added) {
    D_print(F("Ticker slot rejected, service will not run: "));
    D_println(slot);
  }
}

void setupServeTickers() {
  uint8_t slot = 0;
  if (tehybug.serveData.get.active) {
    addServeTicker(slot++, tehybug.serveData.get.frequency, httpGet);
  }
  if (tehybug.serveData.post.active) {
    addServeTicker(slot++, tehybug.serveData.post.frequency, httpPost);
  }
  if (tehybug.serveData.mqtt.active) {
    addServeTicker(slot++, tehybug.serveData.mqtt.frequency, mqttSendData);
  }
  if (tehybug.serveData.ha.active) {
    // HA reports on the MQTT interval
    addServeTicker(slot++, tehybug.serveData.mqtt.frequency, haSendData);
  }
  if (tehybug.serveData.eeprom.active) {
    // the EEPROM log is written inside read_sensors(); a bare tick (no
    // network send) is enough to drive it on its own frequency
    addServeTicker(slot++, tehybug.serveData.eeprom.frequency, [] {});
  }
  if (tehybug.anyScenarioActive()) {
    // Scenarios are evaluated in loop() only on the sleep-mode path, so in live
    // mode they never ran. Give them their own tick against fresh readings, on
    // the shortest configured reporting interval. One dedicated ticker (rather
    // than evaluating inside every service tick) keeps a scenario from firing
    // repeatedly when several services are active.
    addServeTicker(slot++, tehybug.minDataFrequency(), serve_scenario);
  }
}
