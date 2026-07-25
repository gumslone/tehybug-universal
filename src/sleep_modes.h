#pragma once
// Deep / light / modem sleep between measurements.
//
// Expects the following globals (defined in tehybug.ino before this
// header is included): `tehybug`, `mqttClient`.
#include <ESP8266WiFi.h>
#include "debug.h"

// How long to wait for the WiFi association to come back after a light sleep
// before giving up on this round (the next wake tries again).
constexpr unsigned long LIGHT_SLEEP_RECONNECT_TIMEOUT_MS = 8000;

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
  WiFi.disconnect();

  const uint32_t sleep_time_in_ms = 1000 * freq;

  // Enter light sleep
  wifi_set_opmode(NULL_MODE);
  wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
  wifi_fpm_open();
  wifi_fpm_set_wakeup_cb(wakeupCallback);
  wifi_fpm_do_sleep(freq * 1000000);
  delay(sleep_time_in_ms + 1);

  // Wake up and restore WiFi
  wifi_fpm_close();
  wifi_set_opmode(STATION_MODE);
  wifi_set_sleep_type(NONE_SLEEP_T);

  D_println("Woke from light sleep, reconnecting WiFi...");

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
    // the SDK usually re-associates by itself; nudge it once if it has not
    if (!retried && (millis() - start) > 1000) {
      retried = true;
      WiFi.reconnect();
    }
    delay(50);
  }

  if (WiFi.status() == WL_CONNECTED) {
    D_print(F("WiFi back after light sleep, ms: "));
    D_println(millis() - start);
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

void startDeepSleep(int freq) {
  D_println("Going to deep sleep...");
  ESP.deepSleep(freq * 1000000ULL);
  yield();
}

void startSleep(int freq)
{
  tehybug.pixel.off();
  if(tehybug.device.sleepMode)
  {
    startDeepSleep(freq);
  }
  if(tehybug.device.lightSleepMode)
  {
    if(freq >= 30)
    {
      startLightSleep(freq);
    }
    else if(freq >= 10)
    {
      // Modem sleep - WiFi radio sleeps, connection maintained
      // Good for BME680 calibration and 10-30s intervals
      startModemSleep(freq);
    }
    else
    {
      D_println("Frequency too low for light sleep, using normal delay");
      delay(freq * 1000);
    }
  }
}
