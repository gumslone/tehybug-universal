#pragma once
#include <climits>
#include "data_types.h"

// Pure boot/serve decision logic that setup() and loop() hinge on. Kept free
// of hardware so it can be unit-tested on the host (tests/test_mode_logic.cpp);
// the TeHyBug class delegates to these.
namespace mode_logic {

// deep or light sleep configured
inline bool sleepEnabled(const Device &d) {
  return d.sleepMode || d.lightSleepMode;
}

// offline (no-WiFi) mode only engages when the EEPROM is actually present
inline bool offlineEnabled(const Device &d, const Peripherals &p) {
  return d.offlineMode && p.eeprom;
}

// any data-serving or logging mode selected
inline bool anyServeModeActive(const DataServ &s) {
  return s.get.active || s.post.active || s.mqtt.active || s.ha.active ||
         s.eeprom.active;
}

// data logging needs both the RTC (timestamps) and the EEPROM (storage)
inline bool dataLogAvailable(const Peripherals &p) {
  return p.eeprom && p.ds3231;
}

// Smallest configured reporting interval of the active network services, used
// to pick the BME680 sample rate; defaults to 60s when nothing is active. HA
// reports on the MQTT interval; the EEPROM log interval is intentionally not
// included here.
inline int minDataFrequency(const DataServ &s) {
  int minFreq = INT_MAX;
  auto consider = [&](bool active, int freq) {
    if (active && freq > 0 && freq < minFreq) minFreq = freq;
  };
  consider(s.mqtt.active, s.mqtt.frequency);
  consider(s.get.active, s.get.frequency);
  consider(s.post.active, s.post.frequency);
  consider(s.ha.active, s.mqtt.frequency);
  if (minFreq == INT_MAX || minFreq == 0) {
    minFreq = 60;
  }
  return minFreq;
}

} // namespace mode_logic
