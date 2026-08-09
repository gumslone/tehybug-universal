#pragma once
#include <Arduino.h>
#include "data_types.h"

// Pure decision/format logic for the display board's OLED, kept free of
// hardware so it runs in the host tests (tests/test_display_logic.cpp).
// display.h executes these against the RTC and the U8G2 driver.
namespace display_logic {

// Pages the up/down buttons cycle through.
enum : uint8_t { PAGE_CLOCK = 0, PAGE_SENSORS = 1, PAGE_COUNT = 2 };

// Next page for a button press; clamps at the ends like the original
// firmware (no wrap-around, so a page stays reachable by feel).
inline uint8_t nextPage(uint8_t page, int8_t direction) {
  if (direction > 0) {
    return (page + 1 < PAGE_COUNT) ? page + 1 : PAGE_COUNT - 1;
  }
  return (page > 0) ? page - 1 : 0;
}

// Parses "HH:MM" (or "H:MM"). Returns false — leaving the outputs untouched —
// for anything else, so an empty or malformed config value disables the
// feature that uses it instead of firing at a garbage time.
inline bool parseHHMM(const String &text, uint8_t &hour, uint8_t &minute) {
  const int colon = text.indexOf(':');
  if (colon < 1 || colon + 1 >= (int)text.length()) {
    return false;
  }
  const int h = text.substring(0, colon).toInt();
  const int m = text.substring(colon + 1).toInt();
  // toInt() returns 0 for junk; require the digits to actually be digits
  for (unsigned int i = 0; i < text.length(); i++) {
    const char c = text.charAt(i);
    if (c != ':' && (c < '0' || c > '9')) {
      return false;
    }
  }
  if (h < 0 || h > 23 || m < 0 || m > 59) {
    return false;
  }
  hour = h;
  minute = m;
  return true;
}

// RTC wday (1=Sunday..7=Saturday, what /api/settime stores) to ISO
// (1=Monday..7=Sunday, the alarm weekday CSV order). The original display
// firmware indexed its Monday-first weekday array with a Sunday of 0 — an
// out-of-bounds read that made Sunday alarms undefined; this mapping is the
// fix. Out-of-range input returns 0, which alarmDue treats as "never".
inline uint8_t isoWeekday(uint8_t rtcWday) {
  if (rtcWday < 1 || rtcWday > 7) {
    return 0;
  }
  return rtcWday == 1 ? 7 : rtcWday - 1;
}

// Whether an alarm fires at this moment. `weekday` is ISO: 1=Monday..7=Sunday
// (the DS3231 driver's wday convention, set from the browser via
// /api/settime). Fires only in second 0 so one match cannot re-trigger for a
// whole minute; the caller ticks at 1 Hz, so second 0 is always observed.
inline bool alarmDue(const AlarmConf &alarm, uint8_t weekday, uint8_t hour,
                     uint8_t minute, uint8_t second) {
  if (!alarm.active || second != 0 || weekday < 1 || weekday > 7) {
    return false;
  }
  uint8_t alarmHour = 0;
  uint8_t alarmMinute = 0;
  if (!parseHHMM(alarm.time, alarmHour, alarmMinute)) {
    return false;
  }
  if (alarmHour != hour || alarmMinute != minute) {
    return false;
  }
  // weekdays is "1,0,0,1,0,0,0" Monday..Sunday; flag n sits at index 2n
  const unsigned int idx = (weekday - 1) * 2;
  return idx < alarm.weekdays.length() && alarm.weekdays.charAt(idx) == '1';
}

// Whether `now` falls inside the night window [start, end), where a window
// crossing midnight ("22:00".."07:00") covers late evening and early
// morning. start == end means the window never matches (not "always").
inline bool inNightWindow(const String &start, const String &end,
                          uint8_t hour, uint8_t minute) {
  uint8_t sh = 0, sm = 0, eh = 0, em = 0;
  if (!parseHHMM(start, sh, sm) || !parseHHMM(end, eh, em)) {
    return false;
  }
  const int now = hour * 60 + minute;
  const int from = sh * 60 + sm;
  const int to = eh * 60 + em;
  if (from == to) {
    return false;
  }
  if (from < to) {
    return now >= from && now < to;
  }
  return now >= from || now < to; // crosses midnight
}

// The clock page's time, "HH MM" (the blinking colon is drawn separately).
// In 12-hour mode the hour is folded to 1..12; the am/pm marker is
// formatAmPm's.
inline String formatClockTime(uint8_t hour, uint8_t minute, bool h12) {
  uint8_t h = hour;
  if (h12) {
    h = hour % 12;
    if (h == 0) {
      h = 12;
    }
  }
  String out = (h < 10) ? "0" + String(h) : String(h);
  out += " ";
  out += (minute < 10) ? "0" + String(minute) : String(minute);
  return out;
}

// "am"/"pm" in 12-hour mode, empty in 24-hour mode (nothing to draw).
inline String formatAmPm(uint8_t hour, bool h12) {
  if (!h12) {
    return String();
  }
  return hour < 12 ? "am" : "pm";
}

// The first fired alarm's message, or empty when none is showing. A fired
// alarm with an empty message beeps but does not take over the panel — that
// is what the original firmware did, kept as behavior.
inline String firedAlarmMessage(const Alarms &alarms, const bool *fired) {
  for (uint8_t i = 0; i < Alarms::count; i++) {
    if (fired[i] && alarms.items[i].message.length() > 0) {
      return alarms.items[i].message;
    }
  }
  return String();
}

} // namespace display_logic
