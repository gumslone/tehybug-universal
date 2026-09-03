#pragma once
#include "debug.h"
#include "board.h"

// set pin 4 HIGH to turn on the pixel
#ifndef SIGNAL_LED_PIN
#define SIGNAL_LED_PIN 4
#endif

#ifndef PIXEL_ACTIVE
#define PIXEL_ACTIVE 1
#endif

// Whether SIGNAL_LED_PIN gates the pixel's power supply. The display board
// powers its WS2812B permanently and uses GPIO4 for the buzzer instead, so
// driving the "gate" there would sound the buzzer (see board.h).
#ifndef PIXEL_POWER_GATED
#define PIXEL_POWER_GATED 1
#endif
#define PIXEL_COUNT 1 // Number of NeoPixels
#define PIXEL_PIN 12  // Digital IO pin connected to the NeoPixels.

// Settling time after SIGNAL_LED_PIN enables the pixel's supply, before the
// first colour frame is clocked out. Only paid when the pixel was actually off.
#define PIXEL_POWER_UP_MS 3

// On the generic (esp-01) board the indicator sits on GPIO1, which is the UART
// TX pin. Driving it as a GPIO takes the pin away from the serial port, so a
// debug build lit the LED and lost its own log in the same call. In a debug
// build the log is worth more than the indicator, so leave the pin alone.
#if DEBUG && SIGNAL_LED_PIN == 1
#define SIGNAL_LED_ACTIVE 0
#else
#define SIGNAL_LED_ACTIVE 1
#endif

#if PIXEL_ACTIVE
#include <Adafruit_NeoPixel.h>
#endif
class TeHyBugPixel
{
public:
#if PIXEL_ACTIVE
  TeHyBugPixel() : m_neoPixel(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800) {
  }
#endif
  void on(uint8_t r=0, uint8_t g=0, uint8_t b=255, uint8_t brightness=50) {
    D_println("Led on");
#if !PIXEL_POWER_GATED
    // permanently powered pixel: just clock the colour out
    setPixel(r, g, b, brightness);
#elif !SIGNAL_LED_ACTIVE
    (void)r; (void)g; (void)b; (void)brightness;
    return; // the pin is the debug serial line, see SIGNAL_LED_ACTIVE
#else
    pinMode(SIGNAL_LED_PIN, OUTPUT);
    if (SIGNAL_LED_PIN == 1) {
      digitalWrite(SIGNAL_LED_PIN, LOW); // on
    } else {
      const bool wasUnpowered = !m_powered;
      digitalWrite(SIGNAL_LED_PIN, HIGH); // on (SIGNAL_LED_PIN gates its supply)
      m_powered = true;

#if PIXEL_ACTIVE
      // The WS2812 needs its supply to come up and its internal reset to clear
      // before it will latch a frame. Writing the colour in the same breath as
      // enabling power meant the first frame was dropped, so the LED stayed
      // dark until something lit it a second time — which is why it only came
      // on when the MODE button was released, not while it was held.
      if (wasUnpowered) {
        delay(PIXEL_POWER_UP_MS);
      }
      setPixel(r, g, b, brightness);
#endif
    }
#endif
  }

  void off() {
    D_println("Led off");
    // on() configures the pin, but off() is reached on paths where on() never
    // ran (a boot straight into a serving mode, sleep). Writing to a pin still
    // in INPUT mode only toggles its pullup, so the LED was not actually driven
    // off there.
#if !PIXEL_POWER_GATED
    setPixel(0, 0, 0, 0);
#elif !SIGNAL_LED_ACTIVE
    return; // the pin is the debug serial line, see SIGNAL_LED_ACTIVE
#else
    pinMode(SIGNAL_LED_PIN, OUTPUT);
    if (SIGNAL_LED_PIN == 1) {
      digitalWrite(SIGNAL_LED_PIN, HIGH); // off
    } else {
#if PIXEL_ACTIVE
      setPixel(0, 0, 0, 0);
#endif
      digitalWrite(SIGNAL_LED_PIN, LOW); // off (cuts the pixel's supply)
      m_powered = false;
    }
#endif
  }

private:
  // Whether SIGNAL_LED_PIN currently supplies the pixel, so the power-up
  // settling delay is only paid on the transition from off to on.
  bool m_powered{false};

#if PIXEL_ACTIVE

  void setPixel(uint8_t r=0, uint8_t g=0, uint8_t b=255, uint8_t brightness=50) {
        m_neoPixel.begin(); // Initialize NeoPixel strip object (REQUIRED)
        m_neoPixel.setPixelColor(0, m_neoPixel.Color(r, g, b));
        m_neoPixel.setBrightness(brightness);
        m_neoPixel.show(); // Initialize all pixels to 'on'
  }

  Adafruit_NeoPixel m_neoPixel;
#endif
};
