#pragma once
#include "debug.h"
#include "common_functions.h"
#include "configuration.h"

#if defined(ARDUINO_ESP8266_GENERIC)
// The TeHyBug mini has no DS3231 RTC; this stub keeps the call sites
// (data logging, /api/settime) compiling without pulling in the driver.
class RtcTime {
  public:
    RtcTime(TeHyBugConfig &) {}
    void setup() {}
    uint8_t getHours() { return 0; }
    uint8_t getMinutes() { return 0; }
    uint8_t getSeconds() { return 0; }
    uint8_t getMonthDay() { return 0; }
    uint8_t getDay() { return 0; }
    uint8_t getMonth() { return 0; }
    uint16_t getYear() { return 0; }
    bool isTimeSet() { return false; }
    String timestamp() { return String(); }
    void update() {}
    void setTime(uint16_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t,
                 uint8_t) {}
};
#else
#include <ds3231.h>

class RtcTime {
  public:
    RtcTime(TeHyBugConfig & conf): m_conf(conf) {
    }
    void setup()
    {
      DS3231_init(DS3231_CONTROL_INTCN);
    }
    uint8_t getHours() {
      return m_rtcTime.hour;
    }
    uint8_t  getMinutes() {
      return m_rtcTime.min;
    }
    uint8_t getSeconds() {
      return m_rtcTime.sec;
    }
    uint8_t getMonthDay() {
      return m_rtcTime.mday;
    }
    uint8_t getDay() {
      return m_rtcTime.wday;
    }
    // DS3231_get already delivers month 1-12 and the full year
    uint8_t getMonth() {
      return m_rtcTime.mon;
    }
    uint16_t getYear() {
      return m_rtcTime.year;
    }
    // true once the clock has been set (fresh DS3231 chips start at 1900/2000)
    bool isTimeSet() {
      return m_rtcTime.year >= 2024;
    }
    // "YYYY-MM-DD HH:MM" of the last update()
    String timestamp() {
      return String(getYear()) + "-" + IntFormat(getMonth()) + "-" +
             IntFormat(getMonthDay()) + " " + IntFormat(getHours()) + ":" +
             IntFormat(getMinutes());
    }
    void update()
    {
      if(m_conf.rtcActive()){DS3231_get(&m_rtcTime);}
    }
    void setTime(uint16_t year, uint8_t month, uint8_t mday, uint8_t wday,
                 uint8_t hour, uint8_t minute, uint8_t second)
    {
      struct ts t {};
      t.year = year;  // full year, e.g. 2026
      t.mon = month;  // 1-12
      t.mday = mday;
      t.wday = wday;
      t.hour = hour;
      t.min = minute;
      t.sec = second;
      DS3231_set(t);
      update();
      D_println(F("RTC time set"));
    }
  private:
    struct ts m_rtcTime {}; // for ds3231 rtc time
    TeHyBugConfig & m_conf;

};
#endif
