#pragma once
// Clock from the network. The DS3231 keeps time on its battery, but somebody
// had to set it first — from the browser, by hand. With WiFi up anyway, one
// SNTP exchange sets it (and the system clock) in the configured time zone,
// so data-log timestamps and display alarms are right from the first boot.
#include <Arduino.h>
#include <time.h>
#include <ESP8266WiFi.h>
#include "globals.h"
#include "debug.h"

// How long to wait for the first NTP answer. Runs once in setup() with WiFi
// already associated, so this is the whole cost of the feature per boot.
constexpr unsigned long NTP_WAIT_MS = 5000;
// time(nullptr) starts near 0 after boot; anything past this is a real answer
constexpr time_t NTP_EPOCH_SANE = 100000;

void Log(const String &function, const String &message); // web_api.h, later in the sketch

// Sets the system clock, and the RTC when one is fitted, from NTP. Returns
// whether a time was obtained. Requires an associated WiFi connection and
// the setting switched on; silently does nothing otherwise.
bool syncClockFromNtp() {
  if (!tehybug.device.ntpActive || WiFi.status() != WL_CONNECTED) {
    return false;
  }
  // POSIX TZ ("CET-1CEST,M3.5.0,M10.5.0/3"); empty means UTC
  const String tz = tehybug.device.timezone.length() ? tehybug.device.timezone : String(F("UTC0"));
  const String server = tehybug.device.ntpServer.length() ? tehybug.device.ntpServer : String(F("pool.ntp.org"));
  configTime(tz.c_str(), server.c_str());

  const unsigned long start = millis();
  time_t now = time(nullptr);
  while (now < NTP_EPOCH_SANE && millis() - start < NTP_WAIT_MS) {
    delay(50);
    yield();
    now = time(nullptr);
  }
  if (now < NTP_EPOCH_SANE) {
    Log(F("NTP"), String(F("No answer from ")) + server);
    return false;
  }

  struct tm local {};
  localtime_r(&now, &local);
  if (tehybug.peripherals.ds3231) {
    // the RTC's weekday is 1=Sunday..7=Saturday, tm_wday is 0=Sunday
    tehybug.time.setTime(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                         local.tm_wday + 1, local.tm_hour, local.tm_min,
                         local.tm_sec);
  }
  char stamp[24];
  strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &local);
  Log(F("NTP"), String(F("Clock set: ")) + stamp +
                    (tehybug.peripherals.ds3231 ? F(" (RTC updated)") : F(" (no RTC chip)")));
  return true;
}
