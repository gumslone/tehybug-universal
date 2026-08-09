#pragma once
// Debounced polling for the display board's UP/DOWN buttons (active low).
// Small on purpose: the two events this board needs are a click (press +
// release) and a single-shot long press — not worth a vendored library.
//
// The MODE button (GPIO0) is deliberately NOT polled at runtime: it shares
// the I2C SDA line that the OLED and RTC are talking on every second, so it
// stays a boot-time button (config toggle / factory reset, see
// mode_button.h). UP and DOWN sit on free pins and are safe to poll.
#include <Arduino.h>
#include "board.h"

#if TEHYBUG_DISPLAY

class PollButton {
 public:
  // longPressMs 0 disables the long-press event for this button
  explicit PollButton(uint8_t pin, unsigned long longPressMs = 0)
      : m_pin(pin), m_longPressMs(longPressMs) {}

  void begin() {
    pinMode(m_pin, INPUT_PULLUP);
  }

  // Call every loop iteration; the event getters report what this poll found.
  void poll() {
    m_clicked = false;
    m_longPressed = false;
    const bool down = digitalRead(m_pin) == LOW;
    const unsigned long now = millis();
    if (down != m_rawDown) {
      m_rawDown = down;
      m_lastEdgeMs = now;
    }
    if ((now - m_lastEdgeMs) < DEBOUNCE_MS) {
      return; // still bouncing
    }
    if (down && !m_down) {
      m_down = true;
      m_pressedAtMs = now;
      m_longFired = false;
    } else if (down && m_down && !m_longFired && m_longPressMs != 0 &&
               (now - m_pressedAtMs) >= m_longPressMs) {
      // fires once while still held, like EasyButton's onPressedFor
      m_longFired = true;
      m_longPressed = true;
    } else if (!down && m_down) {
      m_down = false;
      if (!m_longFired) {
        m_clicked = true; // a release after a long press is not also a click
      }
    }
  }

  bool clicked() const { return m_clicked; }
  bool longPressed() const { return m_longPressed; }

 private:
  static constexpr unsigned long DEBOUNCE_MS = 40;
  uint8_t m_pin;
  unsigned long m_longPressMs;
  bool m_rawDown{false};
  bool m_down{false};
  bool m_longFired{false};
  bool m_clicked{false};
  bool m_longPressed{false};
  unsigned long m_lastEdgeMs{0};
  unsigned long m_pressedAtMs{0};
};

#endif // TEHYBUG_DISPLAY
