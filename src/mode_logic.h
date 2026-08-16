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
//   OfflineLive - display board only: WiFi off, but stay awake — the clock
//                and sensor pages keep drawing, and the tickers still drive
//                the EEPROM log and IO scenarios. Never sleeps.
enum class DeviceMode : uint8_t {
  Config, Offline, DeepSleep, LightSleep, Live, OfflineLive
};

// alwaysOnDisplay is the display board (TEHYBUG_DISPLAY): its panel must keep
// drawing, so "offline" means WiFi-off-but-awake (OfflineLive) rather than the
// battery boards' log-and-deep-sleep Offline, and the sleep modes — which
// would blank a mains-powered clock — never engage even if a shared config
// carries them.
inline DeviceMode currentMode(const Device &d, const Peripherals &p,
                              bool alwaysOnDisplay = false) {
  if (d.configMode) {
    return DeviceMode::Config;
  }
  if (alwaysOnDisplay) {
    return d.offlineMode ? DeviceMode::OfflineLive : DeviceMode::Live;
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
    case DeviceMode::Config:      return "config";
    case DeviceMode::Offline:     return "offline";
    case DeviceMode::DeepSleep:   return "deep-sleep";
    case DeviceMode::LightSleep:  return "light-sleep";
    case DeviceMode::Live:        return "live";
    case DeviceMode::OfflineLive: return "offline-display";
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

/* Boot decisions ------------------------------------------------------------
 *
 * The precedence rules setup() runs on, extracted so they are pinned by host
 * tests instead of living between hardware calls.
 */

// An offline wake sleeps with the radio left uninitialised (RF_DISABLED),
// which is most of what makes offline mode cheap. If the MODE button just
// switched this boot into config mode, the radio is now needed and cannot be
// brought up without a reset — configMode is persisted, so the restart comes
// straight back into it. One extra boot on a deliberate user action.
inline bool needsRadioRestart(const Device &d, bool fromDeepSleepWake) {
  return d.configMode && d.offlineMode && fromDeepSleepWake;
}

// A device with nothing configured to do must land in config mode — the
// alternative is a fresh device booting into a live mode that serves nothing,
// with WiFi credentials but no way to reach the UI short of the MODE button.
inline bool mustForceConfig(bool firstStart, const DataServ &s) {
  return firstStart || !anyServeModeActive(s);
}

// What setup() brings up after WiFi, resolved from the final config-mode
// decision (i.e. after mustForceConfig has been applied):
//  - config mode runs the web server and nothing else — no MQTT session, no
//    tickers, no remote control, so nothing fights the UI for the heap
//  - MQTT is initialised for plain MQTT and for HA, which rides the same
//    client; HA discovery setup additionally needs ha
//  - tickers only drive live mode: sleep modes serve from loop() directly
struct SetupPlan {
  bool webServer{false};
  bool mqtt{false};
  bool ha{false};
  bool remoteControl{false};
  bool tickers{false};
};

inline SetupPlan setupPlan(const Device &d, const DataServ &s,
                           bool alwaysOnDisplay = false) {
  SetupPlan p;
  if (d.configMode) {
    p.webServer = true;
    return p;
  }
  // Display board with WiFi off: nothing network-facing comes up, but the
  // tickers still run — they drive the EEPROM log and the IO scenarios
  // (setupServeTickers skips the network services in this mode).
  if (alwaysOnDisplay && d.offlineMode) {
    p.tickers = true;
    return p;
  }
  // The display board is mains powered and never sleeps, so its web UI stays
  // up in live mode too: no RESET+MODE dance just to tweak a setting on a
  // device that is awake anyway (only the setup access point goes away).
  // Battery boards keep the web server config-only - their live mode exists
  // to spend as little time awake as possible.
  p.webServer = alwaysOnDisplay;
  p.mqtt = s.mqtt.active || s.ha.active;
  p.ha = s.ha.active;
  p.remoteControl = d.remoteControl.active;
  // the display board never sleeps (see currentMode), so it always tickers
  p.tickers = alwaysOnDisplay || !sleepEnabled(d);
  return p;
}

/* The forced-light-sleep judge -----------------------------------------------
 *
 * wifi_fpm_do_sleep() has two observed failure modes, both invisible in the
 * request itself: it can refuse outright (rc -1 while the association is
 * still tearing down), and it can accept and come straight back (anything
 * calling esp_schedule() releases the one delay() the sleep rides on). Both
 * were found on hardware, as a 60 s interval collapsing to milliseconds.
 *
 * The loop's decisions live here, pure and host-tested; sleep_modes.h only
 * executes them. One verdict per attempted chunk:
 */
enum class SleepVerdict : uint8_t {
  Continue,         // chunk slept (or long enough) - carry on with the rest
  RetryRefused,     // refused, likely "not ready yet": back off, ask again
  AbandonRefused,   // refused too often: it will not sleep, wait the rest out
  AbandonBouncing,  // keeps returning instantly: stop re-arming, wait it out
};

struct SleepJudge {
  uint8_t refusals{0};
  uint8_t shortReturns{0};
};

// A return faster than this did not really sleep.
constexpr unsigned long SLEEP_SHORT_RETURN_MS = 50;
// How many refusals / instant returns before giving up on the SDK for this
// interval. Waiting the remainder out awake is wasteful but keeps the
// reporting interval honest, which matters more than the power for a mode the
// user can switch away from. Counters deliberately never reset within one
// interval: a sleep that bounces, works once, then bounces again is still a
// bouncing sleep.
constexpr uint8_t SLEEP_MAX_REFUSALS = 10;
constexpr uint8_t SLEEP_MAX_SHORT_RETURNS = 5;

inline SleepVerdict judgeSleepChunk(SleepJudge &j, bool refused,
                                    unsigned long chunkElapsedMs) {
  if (refused) {
    return ++j.refusals >= SLEEP_MAX_REFUSALS ? SleepVerdict::AbandonRefused
                                              : SleepVerdict::RetryRefused;
  }
  if (chunkElapsedMs < SLEEP_SHORT_RETURN_MS) {
    return ++j.shortReturns >= SLEEP_MAX_SHORT_RETURNS
               ? SleepVerdict::AbandonBouncing
               : SleepVerdict::Continue;
  }
  return SleepVerdict::Continue;
}

/* The plan for one serve pass: which targets get this measurement, what the
 * MQTT connection needs, and whether the pass ends in sleep. serve_data()
 * used to derive all of this inline, interleaved with the hardware calls -
 * which is exactly where a "drain MQTT even with no link" regression slipped
 * in unnoticed: the rules were not pinned anywhere. Now they are host-tested.
 *
 * The rules, in one place:
 *  - a service sends only when it is active AND the link is up; a dead link
 *    skips the round rather than watching every request fail
 *  - the broker connection is made up front with a retry budget when MQTT or
 *    HA will send (sleep modes get no second loop iteration to reconnect in)
 *  - the pass ends in sleep only when a sleep mode is configured and some
 *    interval is; the interval is the shortest configured one (wakeInterval)
 *  - MQTT disconnects before sleeping whenever it is configured, but the
 *    drain wait is only worth paying when something was actually sent
 */
struct ServePlan {
  bool sendGet{false};
  bool sendPost{false};
  bool sendMqtt{false};
  bool sendHa{false};
  bool connectMqtt{false};     // ensure the broker connection, with a budget
  bool disconnectMqtt{false};  // drop the broker connection before sleeping
  bool drainMqtt{false};       // give the last publishes time to leave
  bool sleep{false};
  int sleepSeconds{0};
};

inline ServePlan servePlan(const DataServ &s, const Device &d, bool linked) {
  ServePlan p;
  p.sendGet = linked && s.get.active;
  p.sendPost = linked && s.post.active;
  p.sendMqtt = linked && s.mqtt.active;
  p.sendHa = linked && s.ha.active;
  p.connectMqtt = p.sendMqtt || p.sendHa;
  if (!sleepEnabled(d)) {
    return p;
  }
  const int freq = wakeInterval(s);
  if (freq <= 0) {
    return p; // nothing scheduled: do not sleep on no timetable
  }
  p.sleep = true;
  p.sleepSeconds = freq;
  p.disconnectMqtt = s.mqtt.active || s.ha.active;
  p.drainMqtt = p.disconnectMqtt && linked;
  return p;
}

// Whether an automation scenario's condition holds for the current reading.
// Lifted out of checkScenario() so the operator semantics - including that an
// unknown operator matches nothing rather than everything - are pinned by
// host tests instead of implied by a boolean chain at the call site.
inline bool scenarioConditionMet(const String &condition, float value,
                                 float threshold) {
  if (condition == "lt") return value < threshold;
  if (condition == "gt") return value > threshold;
  if (condition == "eq") return value == threshold;
  return false;
}

} // namespace mode_logic
