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
//   Config     - the web UI is up. Wins over everything: the MODE button, a
//                first start and "no serving mode selected" all force it.
//   Offline    - no WiFi at all: measure, append to the EEPROM log,
//                deep-sleep. Only reachable when the EEPROM is present.
//   DeepSleep  - measure, serve, then deep-sleep. The chip resets on wake, so
//                the next cycle starts from setup().
//   LightSleep - measure, serve, then light-sleep. Execution resumes inside
//                loop() and the WiFi association has to be re-established,
//                which is why it is a mode of its own and not folded into
//                DeepSleep.
//   Live       - stay awake and serve on tickers.
enum class DeviceMode : uint8_t { Config, Offline, DeepSleep, LightSleep, Live };

inline DeviceMode currentMode(const Device &d, const Peripherals &p) {
  if (d.configMode) {
    return DeviceMode::Config;
  }
  if (offlineEnabled(d, p)) {
    return DeviceMode::Offline;
  }
  // deep sleep first: startSleep() gives it precedence when both are set
  if (d.sleepMode) {
    return DeviceMode::DeepSleep;
  }
  if (d.lightSleepMode) {
    return DeviceMode::LightSleep;
  }
  return DeviceMode::Live;
}

// true for either sleeping mode, where loop() measures and serves itself
// instead of relying on the tickers
inline bool isSleeping(DeviceMode m) {
  return m == DeviceMode::DeepSleep || m == DeviceMode::LightSleep;
}

// for logs and the device-info payload
inline const char *modeName(DeviceMode m) {
  switch (m) {
    case DeviceMode::Config:     return "config";
    case DeviceMode::Offline:    return "offline";
    case DeviceMode::DeepSleep:  return "deep-sleep";
    case DeviceMode::LightSleep: return "light-sleep";
    case DeviceMode::Live:       return "live";
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

// How long a sleeping device rests between wakes: the shortest configured
// interval, so adding a second service cannot starve the first.
//
// The EEPROM log counts, and this is the part that surprises: a 10 s log
// interval means 10 s wakes even when every network service reports hourly,
// which on battery is the difference between months and days. Returns 0 when
// nothing is configured, meaning "do not sleep on a schedule".
inline int wakeInterval(const DataServ &s) {
  int freq = 0;
  if (s.get.active || s.post.active || s.mqtt.active || s.ha.active) {
    freq = minDataFrequency(s);
  }
  if (s.eeprom.active) {
    const int logFreq = s.eeprom.frequency;
    if (freq == 0 || logFreq < freq) {
      freq = logFreq;
    }
  }
  return freq;
}

// Seconds since 2000-01-01 00:00 from a civil date/time, for comparing two
// DS3231 readings across a deep sleep. Days-from-civil after Howard Hinnant's
// algorithm, shifted to a 2000 epoch so uint32 comfortably covers the chip's
// lifetime. Only self-consistency matters here, not any calendar standard.
inline uint32_t epochFromCivil(uint16_t y, uint8_t mo, uint8_t d,
                               uint8_t h, uint8_t mi, uint8_t sec) {
  const int32_t yy = (int32_t)y - (mo <= 2 ? 1 : 0);
  const int32_t era = yy / 400;
  const uint32_t yoe = (uint32_t)(yy - era * 400);
  const uint32_t doy = (153U * (mo + (mo > 2 ? -3 : 9)) + 2U) / 5U + d - 1U;
  const uint32_t doe = yoe * 365U + yoe / 4U - yoe / 100U + doy;
  const int32_t daysSince2000 =
      era * 146097 + (int32_t)doe - 730425;  // days from 2000-01-01
  return (uint32_t)daysSince2000 * 86400UL + h * 3600UL + mi * 60UL + sec;
}

// Whether a deep-sleep wake arrived early enough to mean the reset button
// rather than the timer. The sleep timer runs off the chip's calibrated RC
// oscillator and drifts a few percent, so an hourly sleep waking a minute
// short is normal - the margin scales with the interval (a sixth of it, at
// least 10 s) so drift never reads as a human. A press lands at a uniformly
// random point of the interval, so five sixths of presses are still caught.
inline bool wokeEarly(uint32_t sleptSeconds, uint32_t expectedSeconds) {
  const uint32_t margin =
      expectedSeconds / 6U > 10U ? expectedSeconds / 6U : 10U;
  return sleptSeconds + margin < expectedSeconds;
}

} // namespace mode_logic
