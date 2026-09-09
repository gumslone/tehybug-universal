#pragma once
// The display board's OLED (SH1106 128x64 over I2C), page cycling buttons and
// weekday alarms. Compiled only into the `display` build variant; the page
// content and alarm decisions live in display_logic.h (pure, host-tested),
// this file executes them against the RTC, the buttons and the U8G2 driver.
#include "board.h"

#if TEHYBUG_DISPLAY

#include <U8g2lib.h>
#include "globals.h"
#include "buzzer.h"
#include "common_functions.h"
#include "display_buttons.h"
#include "display_logic.h"
#include "i2cscanner.h"
#include "sensors.h"
#include "web_api.h" // Log()
#include "debug.h"

// Page-buffer mode (the _1_ variant): one 128-byte stripe instead of a 1 KB
// frame buffer, the right trade on a ~40 KB heap that also runs TLS.
//
// The clock/data pins MUST be passed. u8g2.begin() re-runs Wire.begin(), and
// with the pins left at U8X8_PIN_NONE it calls the no-argument form, which on
// ESP8266 re-points the bus at the core defaults (GPIO4/5) — taking the OLED,
// the RTC, the EEPROM and every I2C sensor off the bus that i2cBusBegin() had
// just brought up on GPIO0/2. Passing them makes its Wire.begin(data, clock)
// agree with ours instead.
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset= */ U8X8_PIN_NONE,
                                        /* clock= */ I2C_SCL,
                                        /* data= */ I2C_SDA);

// Holding UP for 10 s toggles offline mode (WiFi off, display stays on) —
// the display board's equivalent of the original firmware's WiFi toggle.
constexpr unsigned long OFFLINE_TOGGLE_HOLD_MS = 10000;
PollButton upButton(UP_BUTTON_PIN, OFFLINE_TOGGLE_HOLD_MS);
PollButton downButton(DOWN_BUTTON_PIN);

// How often the panel redraws (and the RTC is read): the clock's colon
// blinks on this cadence.
constexpr unsigned long DISPLAY_RENDER_MS = 1000;

// How often the display itself triggers a sensor read when no data service
// is doing it more often. Floor of the EEPROM log frequency so the display
// refresh cannot densify the offline log beyond what the user configured.
constexpr int DISPLAY_SENSOR_REFRESH_S = 30;

// The bus clock the sensors are talked to at, restored after every frame.
//
// U8g2 calls Wire.setClock() at the start of each of its transfers and the
// SH1106's descriptor asks for 400 kHz — which then stays set for whatever
// touches the bus next. Most parts here tolerate that, but the AM2320 is a
// 100 kHz device, so leaving the bus overclocked 4x makes exactly one
// supported sensor unreliable. One call per second puts it back.
constexpr uint32_t SENSOR_BUS_CLOCK_HZ = 100000;

byte displayPage = display_logic::PAGE_CLOCK;
bool alarmFired[Alarms::count] = {false, false, false};
// when each alarm started ringing; an alarm nobody mutes stops by itself
unsigned long alarmFiredAtMs[Alarms::count] = {0, 0, 0};
constexpr unsigned long ALARM_AUTO_MUTE_MS = 5UL * 60UL * 1000UL;
// minute-resolution timestamp each alarm last fired at, so one match rings
// once (see display_logic::alarmDue)
String alarmLastFired[Alarms::count];
bool clockColonVisible = true;

const char *const WEEKDAY_NAMES[8] = {"", "Sun", "Mon", "Tue",
                                      "Wed", "Thu", "Fri", "Sat"};

bool anyAlarmFired() {
  for (uint8_t i = 0; i < Alarms::count; i++) {
    if (alarmFired[i]) {
      return true;
    }
  }
  return false;
}

void muteAlarms() {
  for (uint8_t i = 0; i < Alarms::count; i++) {
    alarmFired[i] = false;
  }
  buzzerOff();
}

/* Page rendering ----------------------------------------------------------- */

void drawAlarmPage(const String &message) {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_7x14_tf);
    u8g2.setCursor(0, 15);
    u8g2.print(message);
  } while (u8g2.nextPage());
}

// Right edge, printed bottom-up in the tiny font: the IP address in online
// modes, a wifi-off marker otherwise (the original showed "wifi inactive").
String edgeStatus() {
  if (tehybug.device.offlineMode) {
    return String(F("wifi off"));
  }
  if (!tehybug.displayConf.showIp) {
    return String();
  }
  return WiFi.localIP().toString();
}

// Shown instead of the clock when the RTC has never been set — a fresh
// module, a flat backup battery, or no RTC fitted at all. Rendering the raw
// registers there put "0-00-00 / 00 00" on the panel forever, which looks
// like a broken device rather than one waiting to be told the time; the
// readings still show, so the station stays useful meanwhile.
void drawClockUnsetPage() {
  const String footer =
      tehybug.replacePlaceholders(tehybug.displayConf.line1) + "  " +
      tehybug.replacePlaceholders(tehybug.displayConf.line2);
  // Two lines of what to do next, in reading order: the action, then why.
  // Offline mode has no web interface to point at, so it names the button
  // that brings WiFi back instead of an address that would not answer.
  const String action = tehybug.device.offlineMode
                            ? String(F("hold right btn 10s"))
                            : String(F("set it in the web app:"));
  const String detail = tehybug.device.offlineMode
                            ? String(F("for WiFi, then set it"))
                            : WiFi.localIP().toString();

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_7x14_tf);
    const char *title = "Clock not set";
    u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getUTF8Width(title)) / 2, 20,
                 title);
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(
        (u8g2.getDisplayWidth() - u8g2.getUTF8Width(action.c_str())) / 2, 36,
        action.c_str());
    u8g2.drawStr(
        (u8g2.getDisplayWidth() - u8g2.getUTF8Width(detail.c_str())) / 2, 47,
        detail.c_str());
    u8g2.setCursor(0, 63);
    u8g2.print(footer);
  } while (u8g2.nextPage());
}

void drawClockPage() {
  if (!tehybug.time.isTimeSet()) {
    drawClockUnsetPage();
    return;
  }
  const String date = String(WEEKDAY_NAMES[tehybug.time.getDay() & 7]) + " " +
                      String(tehybug.time.getYear()) + "-" +
                      IntFormat(tehybug.time.getMonth()) + "-" +
                      IntFormat(tehybug.time.getMonthDay());
  const String timeText = display_logic::formatClockTime(
      tehybug.time.getHours(), tehybug.time.getMinutes(),
      tehybug.displayConf.clock12h);
  const String amPm = display_logic::formatAmPm(tehybug.time.getHours(),
                                                tehybug.displayConf.clock12h);
  const String footer =
      tehybug.replacePlaceholders(tehybug.displayConf.line1) + "  " +
      tehybug.replacePlaceholders(tehybug.displayConf.line2);
  const String edge = edgeStatus();

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_7x14_tf);
    u8g2.drawUTF8((u8g2.getDisplayWidth() - u8g2.getUTF8Width(date.c_str())) / 2,
                  15, date.c_str());
    u8g2.setFont(u8g2_font_freedoomr25_mn);
    const int timeX =
        (u8g2.getDisplayWidth() - u8g2.getUTF8Width(timeText.c_str())) / 2;
    u8g2.drawStr(timeX, 50, timeText.c_str());
    if (clockColonVisible) {
      u8g2.drawStr((u8g2.getDisplayWidth() - u8g2.getUTF8Width(":")) / 2, 46,
                   ":");
    }
    if (amPm.length() > 0) {
      u8g2.setFont(u8g2_font_profont10_tr);
      u8g2.drawStr(u8g2.getDisplayWidth() - u8g2.getUTF8Width(amPm.c_str()),
                   30, amPm.c_str());
    }
    if (edge.length() > 0) {
      u8g2.setFont(u8g2_font_micro_tr);
      u8g2.setFontDirection(3); // bottom-up along the right edge
      u8g2.drawStr(128, 63, edge.c_str());
      u8g2.setFontDirection(0);
    }
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.setCursor(0, 63);
    u8g2.print(footer);
  } while (u8g2.nextPage());
}

void drawSensorPage() {
  const String line1 = tehybug.replacePlaceholders(tehybug.displayConf.line1);
  const String line2 = tehybug.replacePlaceholders(tehybug.displayConf.line2);
  const String line3 = tehybug.replacePlaceholders(tehybug.displayConf.line3);
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_7x14_tf);
    u8g2.setCursor(0, 15);
    u8g2.print(line1);
    u8g2.setCursor(0, 33);
    u8g2.print(line2);
    u8g2.setCursor(0, 51);
    u8g2.print(line3);
  } while (u8g2.nextPage());
}

/* Buttons, alarms, the 1 Hz tick ------------------------------------------- */

void handleDisplayButtons() {
  upButton.poll();
  downButton.poll();

  if (upButton.longPressed()) {
    // Toggle offline mode and reboot: WiFi and its services are wired up in
    // setup(), so a clean restart is how the radio state actually changes.
    // Purple LED as the acknowledgement, like the original firmware.
    tehybug.device.offlineMode = !tehybug.device.offlineMode;
    tehybug.conf.saveConfigCallback();
    tehybug.conf.saveConfig();
    tehybug.pixel.on(128, 0, 128);
    delay(1000);
    ESP.restart();
    return;
  }

  const bool up = upButton.clicked();
  const bool down = downButton.clicked();
  if (!up && !down) {
    return;
  }
  buzzerClick();
  if (anyAlarmFired()) {
    // While the siren sounds, the first press of any button just mutes it.
    muteAlarms();
    return;
  }
  displayPage = display_logic::nextPage(displayPage, up ? 1 : -1);
}

void checkAlarms() {
  // An unset clock (fresh DS3231, or none fitted) would sit at a fixed bogus
  // time and could match an alarm forever.
  if (!tehybug.time.isTimeSet()) {
    return;
  }
  const uint8_t weekday = display_logic::isoWeekday(tehybug.time.getDay());
  const String stamp = tehybug.time.timestamp();
  for (uint8_t i = 0; i < Alarms::count; i++) {
    if (display_logic::alarmDue(tehybug.alarms.items[i], weekday,
                                tehybug.time.getHours(),
                                tehybug.time.getMinutes(), stamp,
                                alarmLastFired[i])) {
      alarmFired[i] = true;
      alarmFiredAtMs[i] = millis();
      Log(F("Alarm"), "Alarm " + String(i + 1) + " fired");
    }
  }
  // Ring for at most ALARM_AUTO_MUTE_MS: with nobody home to press a button,
  // an alarm used to sound until the next reboot.
  for (uint8_t i = 0; i < Alarms::count; i++) {
    if (alarmFired[i] && millis() - alarmFiredAtMs[i] >= ALARM_AUTO_MUTE_MS) {
      alarmFired[i] = false;
      Log(F("Alarm"), "Alarm " + String(i + 1) + " stopped by itself");
    }
  }
  // Silence the buzzer once, when the last alarm stops - a noTone() on every
  // tick would clip the short click that a button press plays.
  static bool wasRinging = false;
  const bool ringing = anyAlarmFired();
  if (ringing) {
    buzzerAlarmTick();
  } else if (wasRinging) {
    buzzerOff();
  }
  wasRinging = ringing;
}

// Display-triggered sensor refresh, so the pages show readings even when no
// data service is scheduled (config mode, offline mode with no log).
//
// Gated on lastSensorReadMs — the last read by anyone — so a device already
// measuring on a serve ticker does not also pay a second blocking read here
// (a DHT sample alone holds the loop for seconds, which the clock would show
// as a stutter). Also floored at the log frequency, since read_sensors() is
// what writes the EEPROM log: a faster display refresh would otherwise
// densify the log beyond what was configured.
void refreshDisplaySensors() {
  int refreshS = DISPLAY_SENSOR_REFRESH_S;
  if (tehybug.serveData.eeprom.active &&
      tehybug.serveData.eeprom.frequency > refreshS) {
    refreshS = tehybug.serveData.eeprom.frequency;
  }
  const unsigned long now = millis();
  if (lastSensorReadMs != 0 &&
      (now - lastSensorReadMs) < (unsigned long)refreshS * 1000) {
    return;
  }
  // A DHT read waits out its sampling period through sensorWait(), which
  // calls displayTick() to keep the clock drawing — and displayTick() lands
  // back here. Don't start a second read inside the first.
  static bool reading = false;
  if (reading) {
    return;
  }
  reading = true;
  read_sensors(); // updates lastSensorReadMs itself
  reading = false;
}

// Call from every loop() iteration, in every mode: polls the buttons each
// pass and does the 1 Hz work (RTC read, alarm check, redraw) on its own
// schedule.
void displayTick() {
  handleDisplayButtons();

  static unsigned long lastRenderMs = 0;
  const unsigned long now = millis();
  if (now - lastRenderMs < DISPLAY_RENDER_MS) {
    return;
  }
  lastRenderMs = now;

  tehybug.time.update();
  checkAlarms();
  refreshDisplaySensors();

  // Night mode: blank the panel inside the configured window. Alarms still
  // fire and the buttons still work (mute, page changes) — only the pixels
  // rest. An active siren overrides the blanking so the message is readable.
  if (tehybug.displayConf.nightMode && !anyAlarmFired() &&
      display_logic::inNightWindow(tehybug.displayConf.nightStart,
                                   tehybug.displayConf.nightEnd,
                                   tehybug.time.getHours(),
                                   tehybug.time.getMinutes())) {
    u8g2.setPowerSave(1);
    // setPowerSave is itself an I2C transfer, so u8g2 has just left the bus at
    // its own 400 kHz — restore the sensor clock before returning. Without
    // this, the whole night window ran the bus overclocked (the restore at the
    // end of this function was never reached), which is precisely what the
    // 100 kHz AM2320 cannot take.
    Wire.setClock(SENSOR_BUS_CLOCK_HZ);
    return;
  }
  u8g2.setPowerSave(0);

  const String alarmMessage =
      display_logic::firedAlarmMessage(tehybug.alarms, alarmFired);
  if (alarmMessage.length() > 0) {
    drawAlarmPage(alarmMessage);
  } else if (displayPage == display_logic::PAGE_CLOCK) {
    drawClockPage();
    clockColonVisible = !clockColonVisible;
  } else {
    drawSensorPage();
  }

  // hand the bus back to the sensors at their clock (see the constant)
  Wire.setClock(SENSOR_BUS_CLOCK_HZ);
}

// Bring the panel up and show the boot splash. Call after checkModeButton()
// (GPIO0 doubles as I2C SDA) and after the I2C bus scan, so the splash can
// list what was found.
void displaySetup() {
  upButton.begin();
  downButton.begin();
  // keep the panel alive while a sensor read waits (a DHT sample is ~2 s)
  sensorWaitHook = displayTick;

  u8g2.begin();
  u8g2.enableUTF8Print();

  String addresses;
  i2cScanner::Scanner &scanner = i2cScanner::shared();
  for (uint8_t address = 1; address < 127; address++) {
    if (scanner.addressExists(address)) {
      if (addresses.length() > 0) {
        addresses += " ";
      }
      addresses += "0x" + String(address, HEX);
    }
  }

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_7x14_tf);
    u8g2.drawStr(0, 20, "TeHyBug starting...");
    u8g2.setFont(u8g2_font_profont10_tr);
    u8g2.drawStr(0, 35, wifiSsid);
    u8g2.drawStr(0, 53, "I2C devices:");
    u8g2.drawStr(0, 63, addresses.c_str());
  } while (u8g2.nextPage());
}

#endif // TEHYBUG_DISPLAY
