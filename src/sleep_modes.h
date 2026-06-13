#pragma once
// Deep / light / modem sleep between measurements.
//
// Expects the following globals (defined in tehybug.ino before this
// header is included): `tehybug`, `mqttClient`.
#include <ESP8266WiFi.h>
#include "debug.h"

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

  // WiFi will reconnect via checkWifi() in main loop
  // MQTT will reconnect via mqttReconnect()
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
