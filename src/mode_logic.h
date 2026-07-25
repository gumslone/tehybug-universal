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

// The mode the device is actually operating in, resolved from the stored
// config plus the hardware present. Having one resolver keeps the precedence
// in a single place — it used to be spelled out as ad-hoc boolean combinations
// ("!configMode && !sleepEnabled()", "offlineMode && !configMode", ...) at
// every call site, which is easy to get subtly wrong.
//
// Precedence, highest first:
//   Config  - the web UI is up. Wins over everything: the MODE button, a first
//             start and "no serving mode selected" all force it.
//   Offline - no WiFi at all: measure, append to the EEPROM log, deep-sleep.
//             Only reachable when the EEPROM is actually present.
//   Sleep   - measure, serve, then deep/light sleep.
//   Live    - stay awake and serve on tickers.
enum class DeviceMode : uint8_t { Config, Offline, Sleep, Live };

inline DeviceMode currentMode(const Device &d, const Peripherals &p) {
  if (d.configMode) {
    return DeviceMode::Config;
  }
  if (offlineEnabled(d, p)) {
    return DeviceMode::Offline;
  }
  if (sleepEnabled(d)) {
    return DeviceMode::Sleep;
  }
  return DeviceMode::Live;
}

// for logs and the device-info payload
inline const char *modeName(DeviceMode m) {
  switch (m) {
    case DeviceMode::Config:  return "config";
    case DeviceMode::Offline: return "offline";
    case DeviceMode::Sleep:   return "sleep";
    case DeviceMode::Live:    return "live";
  }
  return "unknown";
}

// any automation scenario configured; used to decide whether live (non-sleep)
// mode needs a ticker to evaluate them
inline bool anyScenarioActive(const Scenarios &sc) {
  for (uint8_t i = 0; i < Scenarios::count; i++) {
    if (sc.items[i].active) {
      return true;
    }
  }
  return false;
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
  // consider() already requires freq > 0, so INT_MAX is the only "nothing
  // active" case left
  if (minFreq == INT_MAX) {
    minFreq = 60;
  }
  return minFreq;
}

} // namespace mode_logic
