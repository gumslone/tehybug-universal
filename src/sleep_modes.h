#pragma once
// Deep / light / modem sleep between measurements.
#include "globals.h"
#include <ESP8266WiFi.h>
#include "debug.h"
#include "wifi_hint.h"

// How long to wait for the WiFi association to come back after a light sleep
// before giving up on this round (the next wake tries again).
constexpr unsigned long LIGHT_SLEEP_RECONNECT_TIMEOUT_MS = 8000;

// The SDK's forced-sleep call takes microseconds in a uint32 and rejects
// anything much above 268 s, so longer intervals are slept in chunks.
constexpr uint32_t FPM_MAX_SLEEP_US = 260UL * 1000000UL;
constexpr unsigned long FPM_MAX_SLEEP_MS = 260UL * 1000UL;

// The SDK needs a moment after the association is torn down before it will
// accept a forced sleep; without this the first call answers -1.
constexpr unsigned long FPM_SETTLE_MS = 20;
// Back-off before re-asking after a refusal. When to retry versus give up is
// mode_logic::judgeSleepChunk()'s call, not this file's.
constexpr unsigned long FPM_RETRY_MS = 50;

/* Boot mark ----------------------------------------------------------------
 *
 * One RTC word meaning "the previous boot never reached deep sleep". Written at
 * boot, cleared as the very last thing before ESP.deepSleep().
 *
 * Why the reset reason is not enough on its own: on these boards a deep-sleep
 * wake is a pulse on the same RST pin the reset button pulls, and a press
 * while the device is asleep can be misreported as a deep-sleep wake - which
 * is precisely when someone trying to reach config mode presses it, since a
 * sleeping node is asleep almost all the time. A "going to sleep" flag cannot
 * tell those apart either (it is set for both). This mark can: a timer wake
 * always follows a completed sleep entry, so it always finds the mark cleared,
 * while a second reset inside the short awake window finds it set - only a
 * human resets twice within seconds. Worst case becomes "press reset twice,
 * then MODE", instead of a window that never appears.
 */
constexpr uint32_t BOOT_MARK_RTC_SLOT = 50;  // WifiHint ends at 47, HA memo at 64
constexpr uint32_t BOOT_MARK_MAGIC = 0x4D4F4445;  // 'MODE'

bool bootMarkPresent() {
  uint32_t v = 0;
  return ESP.rtcUserMemoryRead(BOOT_MARK_RTC_SLOT, &v, sizeof(v)) &&
         v == BOOT_MARK_MAGIC;
}

void setBootMark() {
  uint32_t v = BOOT_MARK_MAGIC;
  ESP.rtcUserMemoryWrite(BOOT_MARK_RTC_SLOT, &v, sizeof(v));
}

void clearBootMark() {
  uint32_t v = 0;
  ESP.rtcUserMemoryWrite(BOOT_MARK_RTC_SLOT, &v, sizeof(v));
}

void wakeupCallback()
{
  D_println("Light sleep callback...");
}

void startLightSleep(int freq)
{
  // Light sleep only makes sense for frequencies >= 30 seconds
  // Below that, the overhead of sleep/wake isn't worth it
  if (freq < 30) {
    D_println("Frequency too low for light sleep, using normal delay");
    delay(freq * 1000);
    return;
  }
  D_println("Going to light sleep...");

  // Disconnect WiFi gracefully
  if(mqttClient.connected()) {
    mqttClient.disconnect();
  }
  // Deliberately NOT WiFi.disconnect(): the core implements that by writing an
  // empty station_config, which erases the stored SSID and password. Nothing
  // could then re-associate after the sleep, which is why every request after
  // the first wake failed on a dead link. The SDK call drops the association
  // without touching the saved credentials.
  wifi_station_disconnect();

  // Enter light sleep.
  //
  // _current, not wifi_set_opmode(): the persistent form writes the mode to
  // flash on every single sleep, which is both flash wear and time, and the
  // mode is meant to last only until the wake below restores it.
  wifi_set_opmode_current(NULL_MODE);
  wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
  wifi_fpm_open();
  wifi_fpm_set_wakeup_cb(wakeupCallback);

  // Let the teardown settle before asking to sleep. wifi_fpm_do_sleep() answers
  // -1 while the association is still coming down — observed on hardware as
  // "fpm_do_sleep refused, rc: -1" on the very first call, with the identical
  // argument accepted on other boots. The retry loop below covers the rest.
  delay(FPM_SETTLE_MS);

  // The SDK's forced sleep takes microseconds in a uint32 and tops out around
  // 268 s, so a longer interval has to be slept in chunks. (The old code
  // computed `freq * 1000000` in a signed int, which also overflowed above
  // ~2147 s.)
  // Sleep until the requested time has actually elapsed, rather than trusting a
  // single delay() to last.
  //
  // wifi_fpm_do_sleep() can return 0 (accepted) and still come straight back:
  // delay() on this core arms a timer and yields exactly once, so anything else
  // calling esp_schedule() — a WiFi disconnect event still in flight from the
  // wifi_station_disconnect() above, for instance — releases it early. The old
  // loop subtracted the whole chunk regardless and exited, so a 60 s sleep could
  // return in 1 ms ("actually slept: 1") and every service then ran ~60x too
  // often. Re-issuing the sleep for whatever is left makes an early release cost
  // nothing.
  const unsigned long targetMs = (unsigned long)freq * 1000UL;
  const unsigned long sleepStart = millis();
  mode_logic::SleepJudge judge;
  bool abandoned = false;
  while (!abandoned && millis() - sleepStart < targetMs) {
    const unsigned long leftMs = targetMs - (millis() - sleepStart);
    const uint32_t chunkUs = (leftMs > FPM_MAX_SLEEP_MS)
                                 ? FPM_MAX_SLEEP_US
                                 : (uint32_t)leftMs * 1000UL;
    const unsigned long chunkStart = millis();
    const sint8 rc = wifi_fpm_do_sleep(chunkUs);
    if (rc == 0) {
      delay(chunkUs / 1000UL + 1UL);
    }
    // The verdict logic is pure and host-tested (mode_logic::judgeSleepChunk);
    // this loop only carries it out.
    switch (mode_logic::judgeSleepChunk(judge, rc != 0,
                                        millis() - chunkStart)) {
      case mode_logic::SleepVerdict::Continue:
        break;
      case mode_logic::SleepVerdict::RetryRefused:
        delay(FPM_RETRY_MS);
        break;
      case mode_logic::SleepVerdict::AbandonRefused:
        D_print(F("  fpm_do_sleep refused, rc: "));
        D_println(rc);
        abandoned = true;
        break;
      case mode_logic::SleepVerdict::AbandonBouncing:
        D_println(F("  light sleep keeps returning early, waiting it out"));
        abandoned = true;
        break;
    }
  }
  if (abandoned) {
    // The SDK will not sleep this interval. Waiting the rest out awake is
    // wasteful, but it keeps the reporting interval honest, which matters
    // more than the power for a mode the user can switch away from.
    delay(targetMs - (millis() - sleepStart));
  }
  // What the cycle actually cost, against what was asked for. A wake that comes
  // back far short of the target means the forced sleep is being cut off, and
  // every service then reports that much more often than it was configured to.
  const unsigned long sleptMs = millis() - sleepStart;
  D_print(F("Light sleep asked (ms): "));
  D_print((unsigned long)freq * 1000UL);
  D_print(F("  actually slept: "));
  D_println(sleptMs);

  // Wake up and restore WiFi
  wifi_fpm_close();
  wifi_set_opmode_current(STATION_MODE);
  wifi_set_sleep_type(NONE_SLEEP_T);

  // Re-associate straight to the AP we were on, instead of scanning for it.
  //
  // wifi_station_connect() starts a full scan on every wake — visible in the
  // log as "scandone" before each reconnect, and measured at 300-900 ms of
  // radio-on time per cycle. The channel and BSSID have not changed across a
  // sleep of a few minutes, and they are already in RTC memory for the
  // deep-sleep path, so pin them and skip the scan. If the hint is missing or
  // the AP really did move, the association fails and the retry below falls
  // back to a normal scan, which is the same recovery the boot path uses.
  WifiHint hint;
  bool pinned = false;
  if (loadWifiHint(hint)) {
    const String ssid = WiFi.SSID();
    const String psk = WiFi.psk();
    if (ssid.length() > 0) {
      // Suppress persistence: these are the credentials the SDK already has,
      // so letting the core write them back is a flash erase on every wake.
      const bool wasPersistent = WiFi.getPersistent();
      WiFi.persistent(false);
      WiFi.begin(ssid.c_str(), psk.c_str(), hint.channel, hint.bssid, true);
      WiFi.persistent(wasPersistent);
      pinned = true;
    }
  }
  if (!pinned) {
    wifi_station_connect(); // re-associate with the credentials we kept
  }

  D_print(F("Woke from light sleep, reconnecting WiFi"));
  D_println(pinned ? F(" (pinned to the cached AP)") : F(" (scanning)"));

  // Wait for the link to come back before returning to the caller, which
  // serves data immediately.
  //
  // This used to just note that "checkWifi() in the main loop" would
  // reconnect — but that call sits in the live-mode-only branch of loop(), so
  // in light sleep nothing ever reconnected and every request after the first
  // wake failed on a dead link (HTTP -1, "connection failed"). MQTT still
  // reconnects on its own via mqttReconnect().
  const unsigned long start = millis();
  bool retried = false;
  while (WiFi.status() != WL_CONNECTED &&
         (millis() - start) < LIGHT_SLEEP_RECONNECT_TIMEOUT_MS) {
    // The SDK usually re-associates by itself; nudge it once if it has not.
    // For a pinned attempt this is also the fallback when the AP has moved
    // channel or BSSID: reconnect() drops the pinning and scans normally.
    if (!retried && (millis() - start) > 1000) {
      retried = true;
      if (pinned) {
        D_println(F("  pinned reconnect stalled, falling back to a scan"));
        clearWifiHint(); // it is stale; the boot path will rebuild it
      }
      WiFi.reconnect();
    }
    delay(50);
  }

  if (WiFi.status() == WL_CONNECTED) {
    D_print(F("WiFi back after light sleep, ms: "));
    D_println(millis() - start);
    // Refresh the hint from what actually worked. Without this the light-sleep
    // path would only ever read it, so the one time a stale hint was cleared
    // above, every later wake would scan with nothing left to rebuild it.
    // The counter restarts at 0 because this path re-runs DHCP: it applies no
    // static address, unlike the boot fast path.
    saveWifiHint();
  } else {
    D_println(F("WiFi did not return after light sleep; skipping this round"));
  }
}

void startModemSleep(int freq)
{
  D_println("Going to modem sleep for " + String(freq) + " seconds...");

  // Enable modem sleep - WiFi radio sleeps between beacons
  // Connection maintained, sensor calibration continues
  wifi_set_sleep_type(MODEM_SLEEP_T);

  // Sleep for the specified duration
  delay(freq * 1000);

  // WiFi automatically wakes when needed
}

// wakeMode defaults to RF_DEFAULT because the serving modes need the radio on
// the next boot. Offline mode passes RF_DISABLED: it never transmits, so
// powering and calibrating the radio on wake only to switch it off again in
// setup() is pure cost. Callers that disable RF must make sure anything needing
// the radio on that boot resets first — see the guard in setup().
void startDeepSleep(int freq, RFMode wakeMode = RF_DEFAULT) {
  D_println("Going to deep sleep...");
  // Last thing before sleeping: the next boot finding this cleared is what
  // certifies it as a timer wake (see the boot mark above).
  clearBootMark();
  ESP.deepSleep(freq * 1000000ULL, wakeMode);
  yield();
}

// Below this interval it costs less to stay associated than to drop the link
// and pay for a re-association on the next wake.
//
// Rough ESP8266 figures: re-associating draws ~80 mA, modem sleep (association
// kept, radio napping between beacons) ~15 mA, forced light sleep (association
// dropped) ~1 mA. Per cycle of length T with a re-association taking R:
//     stay connected : 15*T
//     light sleep    : 1*T + 80*R
// so light sleep only wins once T is roughly 5.6*R. At R ~= 5 s that is ~28 s,
// which is where this 30 s comes from. On a network that associates in 2-3 s
// the crossover drops to ~11-17 s and this could be lowered — the debug build
// prints the measured value ("WiFi back after light sleep, ms").
constexpr int STAY_CONNECTED_BELOW_S = 30;

// Under this the radio never really gets to sleep between reads, so just wait.
constexpr int MODEM_SLEEP_MIN_S = 10;

void startSleep(int freq)
{
  tehybug.pixel.off();
  if(tehybug.device.sleepMode)
  {
    startDeepSleep(freq);
  }
  if(tehybug.device.lightSleepMode)
  {
    if(freq >= STAY_CONNECTED_BELOW_S)
    {
      // long enough that dropping the link and re-associating still wins
      startLightSleep(freq);
    }
    else if(freq >= MODEM_SLEEP_MIN_S)
    {
      // Modem sleep - WiFi radio sleeps, connection maintained.
      // Cheaper than reconnecting at this interval, and it keeps BME680
      // calibration running.
      startModemSleep(freq);
    }
    else
    {
      D_println("Frequency too low for light sleep, using normal delay");
      delay(freq * 1000);
    }
  }
}
