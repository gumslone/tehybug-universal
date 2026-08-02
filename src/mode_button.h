#pragma once
// The MODE button (config-mode toggle, factory reset) and the config LED.
// The window policy — when a boot waits for a press and for how long — lives
// here with the code that runs it; the boot mark it consults is
// sleep_modes.h's, since deep-sleep entry is what clears it.
#include "globals.h"
#include "sleep_modes.h"
#include "web_api.h"
#include "debug.h"

void toggleConfigMode() {
  D_println(F("Config mode changed"));
  tehybug.device.configMode = !tehybug.device.configMode;
  if (tehybug.device.configMode) {
    D_println(F("Config mode activated"));
  } else {
    D_println(F("Config mode deactivated"));
  }
  tehybug.conf.saveConfigCallback();
  tehybug.conf.saveConfig();
  yield();
}

// Drive the signal LED to match config mode: blue while configuring, off in
// any serving / sleep mode. Call this after any change to configMode so the
// LED always reflects the current state.
void updateConfigLed() {
  if (tehybug.device.configMode) {
    tehybug.pixel.on();
  } else {
    tehybug.pixel.off();
  }
}

// How long to wait for a MODE-button press after boot.
//
// The button cannot be held down during reset — GPIO0 low at reset puts the ESP
// into flash mode — so it has to be pressed just *after* the device boots,
// which means the firmware has to wait for it rather than sample the pin once.
//
// Live (non-sleep) modes only boot on a manual reset or power-up, so waiting
// costs nothing there and the window is generous. Sleep and offline modes run
// this on every wake-up, where it is battery time, so theirs stays short.
constexpr unsigned long BUTTON_WINDOW_LIVE_MS = 1000;
constexpr unsigned long BUTTON_WINDOW_SLEEP_MS = 1000;

// How long the MODE button must be held to trigger a factory reset. Long
// enough that it cannot be hit by the short press that toggles config mode.
constexpr unsigned long FACTORY_RESET_HOLD_MS = 20000;

// Short press toggles config mode, holding for 20 seconds factory-resets.
//
// Without WiFi (offline mode) or with the web server off (live mode), the MODE
// button is the only way back into config mode. The window used to apply to
// offline mode only, leaving live and deep-sleep boots with just the 100 ms
// settle delay — so the press had to land in a fraction of a second.
void checkModeButton() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  delay(100);

  // Already in config mode? Nothing to switch to, so don't delay the boot.
  //
  // Nor on a deep-sleep wake. The window exists so that a press shortly after a
  // reset has somewhere to land, but a sleeping node is only awake a few
  // seconds an hour — nobody can time a press into one of those, so in practice
  // the button is pressed after a RESET. Paying the window on every wake cost
  // ~1.1 s of radio-on time per cycle, which against a ~24 uA sleep floor is
  // roughly half the energy budget of a short wake. A button already held down
  // is still honoured below; only the waiting is skipped.
  // A boot mark left over means the previous boot never reached deep sleep -
  // either a human just reset the device (possibly the second press after one
  // that was swallowed as a "deep-sleep wake"), or it crashed. Both deserve
  // the window; only a certified timer wake skips it.
  const bool humanReset = bootMarkPresent();
  setBootMark();
  const bool fromDeepSleep =
      !humanReset &&
      ESP.getResetInfoPtr()->reason == REASON_DEEP_SLEEP_AWAKE;
  D_print(F("Reset reason: "));
  D_println(ESP.getResetReason());
  if (!fromDeepSleep && !tehybug.device.configMode &&
      digitalRead(BUTTON_PIN) == HIGH) {
    const bool wakesOften = tehybug.sleepEnabled() || tehybug.device.offlineMode;
    const unsigned long window =
        wakesOften ? BUTTON_WINDOW_SLEEP_MS : BUTTON_WINDOW_LIVE_MS;
    D_print(F("MODE button window (ms): "));
    D_println(window);
    const unsigned long start = millis();
    while (digitalRead(BUTTON_PIN) == HIGH &&
           (millis() - start) < window) {
      delay(10);
    }
  }

  if (digitalRead(BUTTON_PIN) == LOW) {
    const unsigned long pressed = millis();
    bool toggled = false;
    delay(300);
    if (digitalRead(BUTTON_PIN) == LOW) {
      while (digitalRead(BUTTON_PIN) == LOW) {
        if(!toggled)
        {
          toggled = true;
          toggleConfigMode();
          updateConfigLed();
        }
        delay(10);
        if((millis() - pressed) >= FACTORY_RESET_HOLD_MS)
        {
          handleFactoryReset();
        }
      }
    }
  }

  updateConfigLed();
}
