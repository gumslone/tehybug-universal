#pragma once
// The display board's piezo buzzer (BUZZER_PIN): a short acknowledgement
// click for button presses and the two-tone alarm siren. tone() runs off a
// hardware timer, so nothing here blocks the loop — the original firmware
// followed every click with a delay(200), which froze the display and the
// other buttons for that long.
#include <Arduino.h>
#include "board.h"

#if TEHYBUG_DISPLAY

// Button acknowledgement: a short middle C.
constexpr unsigned int BUZZER_CLICK_HZ = 262;
constexpr unsigned long BUZZER_CLICK_MS = 60;

// The alarm alternates between these two tones, one change per second.
constexpr unsigned int BUZZER_ALARM_HIGH_HZ = 780;
constexpr unsigned int BUZZER_ALARM_LOW_HZ = 500;
constexpr unsigned long BUZZER_ALARM_TONE_MS = 180;

inline void buzzerClick() {
  tone(BUZZER_PIN, BUZZER_CLICK_HZ, BUZZER_CLICK_MS);
}

// One siren step; call at 1 Hz while an alarm is firing.
inline void buzzerAlarmTick() {
  static bool high = true;
  tone(BUZZER_PIN, high ? BUZZER_ALARM_HIGH_HZ : BUZZER_ALARM_LOW_HZ,
       BUZZER_ALARM_TONE_MS);
  high = !high;
}

inline void buzzerOff() {
  noTone(BUZZER_PIN);
}

#endif // TEHYBUG_DISPLAY
