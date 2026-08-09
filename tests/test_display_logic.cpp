// Native host tests for the display board's page/alarm/clock decisions
// (display_logic.h) and the display-board mode resolution (mode_logic.h with
// alwaysOnDisplay set).
#include "test_framework.h"
#include "../src/display_logic.h"
#include "../src/mode_logic.h"

using namespace display_logic;

static void test_next_page() {
  CASE("nextPage clamps at both ends");
  CHECK(nextPage(PAGE_CLOCK, 1) == PAGE_SENSORS);
  CHECK(nextPage(PAGE_SENSORS, -1) == PAGE_CLOCK);
  CHECK(nextPage(PAGE_SENSORS, 1) == PAGE_SENSORS); // no wrap up
  CHECK(nextPage(PAGE_CLOCK, -1) == PAGE_CLOCK);    // no wrap down
}

static void test_parse_hhmm() {
  CASE("parseHHMM accepts HH:MM and H:MM");
  uint8_t h = 99, m = 99;
  CHECK(parseHHMM("07:30", h, m));
  CHECK(h == 7);
  CHECK(m == 30);
  CHECK(parseHHMM("7:05", h, m));
  CHECK(h == 7);
  CHECK(m == 5);
  CHECK(parseHHMM("23:59", h, m));
  CHECK(h == 23);
  CHECK(m == 59);
  CHECK(parseHHMM("00:00", h, m));
  CHECK(h == 0);
  CHECK(m == 0);

  CASE("parseHHMM rejects junk and leaves outputs untouched");
  h = 42;
  m = 43;
  CHECK(!parseHHMM("", h, m));
  CHECK(!parseHHMM("12", h, m));
  CHECK(!parseHHMM(":30", h, m));
  CHECK(!parseHHMM("12:", h, m));
  CHECK(!parseHHMM("24:00", h, m));
  CHECK(!parseHHMM("12:60", h, m));
  CHECK(!parseHHMM("aa:bb", h, m));
  CHECK(!parseHHMM("1a:30", h, m));
  CHECK(h == 42); // untouched on every failure
  CHECK(m == 43);
}

static void test_iso_weekday() {
  CASE("isoWeekday maps RTC 1=Sun..7=Sat to ISO 1=Mon..7=Sun");
  CHECK(isoWeekday(1) == 7); // Sunday
  CHECK(isoWeekday(2) == 1); // Monday
  CHECK(isoWeekday(6) == 5); // Friday
  CHECK(isoWeekday(7) == 6); // Saturday
  CHECK(isoWeekday(0) == 0); // unknown -> never
  CHECK(isoWeekday(8) == 0);
}

static AlarmConf makeAlarm(const char *time, const char *weekdays) {
  AlarmConf a;
  a.active = true;
  a.time = time;
  a.weekdays = weekdays;
  return a;
}

static void test_alarm_due() {
  CASE("alarmDue fires on the matching minute, weekday and second 0");
  const AlarmConf a = makeAlarm("07:30", "1,0,0,0,0,0,0"); // Monday only
  CHECK(alarmDue(a, 1, 7, 30, 0));   // Monday 07:30:00
  CHECK(!alarmDue(a, 1, 7, 30, 1));  // only in second 0, else it re-fires
  CHECK(!alarmDue(a, 1, 7, 31, 0));  // wrong minute
  CHECK(!alarmDue(a, 1, 8, 30, 0));  // wrong hour
  CHECK(!alarmDue(a, 2, 7, 30, 0));  // Tuesday: not scheduled

  CASE("alarmDue: Sunday sits at the CSV's last slot");
  const AlarmConf sunday = makeAlarm("09:00", "0,0,0,0,0,0,1");
  CHECK(alarmDue(sunday, 7, 9, 0, 0));  // ISO 7 = Sunday
  CHECK(!alarmDue(sunday, 6, 9, 0, 0)); // Saturday

  CASE("alarmDue: inactive, malformed and out-of-range never fire");
  AlarmConf off = makeAlarm("07:30", "1,1,1,1,1,1,1");
  off.active = false;
  CHECK(!alarmDue(off, 1, 7, 30, 0));
  const AlarmConf junkTime = makeAlarm("junk", "1,1,1,1,1,1,1");
  CHECK(!alarmDue(junkTime, 1, 7, 30, 0));
  const AlarmConf empty = makeAlarm("07:30", "");
  CHECK(!alarmDue(empty, 1, 7, 30, 0));   // no weekday CSV: never
  const AlarmConf all = makeAlarm("07:30", "1,1,1,1,1,1,1");
  CHECK(!alarmDue(all, 0, 7, 30, 0));     // unknown weekday (isoWeekday 0)
  CHECK(!alarmDue(all, 8, 7, 30, 0));
}

static void test_night_window() {
  CASE("inNightWindow: plain window, start inclusive, end exclusive");
  CHECK(inNightWindow("13:00", "15:00", 13, 0));
  CHECK(inNightWindow("13:00", "15:00", 14, 59));
  CHECK(!inNightWindow("13:00", "15:00", 15, 0));
  CHECK(!inNightWindow("13:00", "15:00", 12, 59));

  CASE("inNightWindow: crossing midnight covers evening and morning");
  CHECK(inNightWindow("22:00", "07:00", 23, 30));
  CHECK(inNightWindow("22:00", "07:00", 3, 0));
  CHECK(inNightWindow("22:00", "07:00", 22, 0));
  CHECK(!inNightWindow("22:00", "07:00", 7, 0));
  CHECK(!inNightWindow("22:00", "07:00", 12, 0));

  CASE("inNightWindow: degenerate and malformed windows never match");
  CHECK(!inNightWindow("07:00", "07:00", 7, 0)); // start == end: never
  CHECK(!inNightWindow("", "07:00", 3, 0));
  CHECK(!inNightWindow("22:00", "junk", 23, 0));
}

static void test_clock_format() {
  CASE("formatClockTime 24h");
  CHECK_EQ_STR(formatClockTime(0, 5, false).c_str(), "00 05");
  CHECK_EQ_STR(formatClockTime(23, 59, false).c_str(), "23 59");

  CASE("formatClockTime 12h folds 0->12 and 13->01");
  CHECK_EQ_STR(formatClockTime(0, 15, true).c_str(), "12 15");
  CHECK_EQ_STR(formatClockTime(12, 0, true).c_str(), "12 00");
  CHECK_EQ_STR(formatClockTime(13, 5, true).c_str(), "01 05");
  CHECK_EQ_STR(formatClockTime(23, 0, true).c_str(), "11 00");

  CASE("formatAmPm only exists in 12h mode");
  CHECK_EQ_STR(formatAmPm(0, true).c_str(), "am");
  CHECK_EQ_STR(formatAmPm(11, true).c_str(), "am");
  CHECK_EQ_STR(formatAmPm(12, true).c_str(), "pm");
  CHECK_EQ_STR(formatAmPm(23, true).c_str(), "pm");
  CHECK_EQ_STR(formatAmPm(23, false).c_str(), "");
}

static void test_fired_alarm_message() {
  CASE("firedAlarmMessage: first fired alarm with a message wins");
  Alarms alarms;
  alarms.items[0].message = "first";
  alarms.items[1].message = "";      // fired but silent on screen
  alarms.items[2].message = "third";

  bool fired[Alarms::count] = {false, false, false};
  CHECK_EQ_STR(firedAlarmMessage(alarms, fired).c_str(), "");
  fired[1] = true; // empty message: beeps, but takes no page
  CHECK_EQ_STR(firedAlarmMessage(alarms, fired).c_str(), "");
  fired[2] = true;
  CHECK_EQ_STR(firedAlarmMessage(alarms, fired).c_str(), "third");
  fired[0] = true;
  CHECK_EQ_STR(firedAlarmMessage(alarms, fired).c_str(), "first");
}

static void test_display_board_modes() {
  CASE("display board: offline means awake with WiFi off, never deep sleep");
  Device d;
  Peripherals p;
  d.configMode = false;
  d.offlineMode = true;
  // no EEPROM present: a battery board would fall through to Live...
  CHECK(mode_logic::currentMode(d, p) == mode_logic::DeviceMode::Live);
  // ...but the display board goes to OfflineLive, EEPROM or not
  CHECK(mode_logic::currentMode(d, p, true) ==
        mode_logic::DeviceMode::OfflineLive);
  p.eeprom = true;
  CHECK(mode_logic::currentMode(d, p, true) ==
        mode_logic::DeviceMode::OfflineLive);

  CASE("display board: sleep modes from a shared config never engage");
  d.offlineMode = false;
  d.sleepMode = true;
  CHECK(mode_logic::currentMode(d, p, true) == mode_logic::DeviceMode::Live);
  d.lightSleepMode = true;
  CHECK(mode_logic::currentMode(d, p, true) == mode_logic::DeviceMode::Live);

  CASE("display board: config mode still wins over everything");
  d.configMode = true;
  d.offlineMode = true;
  CHECK(mode_logic::currentMode(d, p, true) == mode_logic::DeviceMode::Config);

  CASE("setupPlan for the display board");
  d.configMode = false;
  d.sleepMode = false;
  d.lightSleepMode = false;
  d.offlineMode = true;
  DataServ s;
  s.mqtt.active = true;
  // offline: nothing network-facing, but the tickers run
  mode_logic::SetupPlan plan = mode_logic::setupPlan(d, s, true);
  CHECK(!plan.webServer);
  CHECK(!plan.mqtt);
  CHECK(!plan.ha);
  CHECK(!plan.remoteControl);
  CHECK(plan.tickers);
  // online display board: normal services, and tickers even if a shared
  // config carries a sleep mode (the display board never sleeps)
  d.offlineMode = false;
  d.sleepMode = true;
  plan = mode_logic::setupPlan(d, s, true);
  CHECK(plan.mqtt);
  CHECK(plan.tickers);
  // the same config on a battery board serves from loop(), not tickers
  plan = mode_logic::setupPlan(d, s);
  CHECK(!plan.tickers);
}

int main() {
  std::printf("Running display_logic tests...\n");
  test_next_page();
  test_parse_hhmm();
  test_iso_weekday();
  test_alarm_due();
  test_night_window();
  test_clock_format();
  test_fired_alarm_message();
  test_display_board_modes();
  return SUMMARY();
}
