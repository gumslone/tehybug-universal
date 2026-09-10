#pragma once
// The RTC-cached WiFi hint: channel, BSSID and address from the last
// successful connection.
//
// Lives in its own header because both the boot path (wifi_service.h) and the
// light-sleep wake path (sleep_modes.h) re-associate from it, and sleep_modes.h
// is included first.
#include <ESP8266WiFi.h>
#include "debug.h"
#include "wifi_policy.h"

/* WiFi fast reconnect ------------------------------------------------------
 *
 * A full scan + association + DHCP takes a few seconds at ~80 mA, and a
 * deep-sleep device pays it on every wake — which dominates its power budget,
 * since the sleep itself is only ~20 uA. Caching the channel, BSSID and IP
 * from the last successful connection lets the next wake skip both the scan
 * and DHCP, typically well under a second instead of several.
 *
 * The cache lives in RTC memory: it survives deep sleep and is wiped by a power
 * cycle, which is the right lifetime for it. Offsets 0-31 belong to eboot when
 * OTA is in play and 64+ holds the HA discovery memo, so this sits at 40.
 *
 * A hint goes stale whenever the AP changes channel, the router hands out a
 * different subnet, or another access point answers for the same SSID — so a
 * failed fast attempt clears the cache and falls back to the normal scan.
 * Otherwise this would trade battery life for a device that cannot reconnect.
 */
constexpr uint32_t WIFI_HINT_RTC_SLOT = 40;
constexpr uint32_t WIFI_HINT_MAGIC = 0x57494649;  // 'WIFI'
constexpr unsigned long WIFI_HINT_TIMEOUT_MS = 4000;

// How long to let the SDK's own auto-connect finish before taking over. It
// starts at boot and normally associates well inside this.
constexpr unsigned long WIFI_SDK_GRACE_MS = 1500;

struct WifiHint {
  uint32_t magic;
  uint32_t crc;
  uint32_t ip;
  uint32_t gw;
  uint32_t mask;
  uint32_t dns;
  uint8_t bssid[6];
  uint8_t channel;
  // wakes since the address last came from DHCP (see wifi_policy.h)
  uint8_t wakesSinceDhcp;
  // signal strength at the last connection, for the automatic transmit
  // power (0 = unknown)
  int8_t rssi;
};

// RTC user memory is not checksummed by the SDK and holds garbage on a cold
// boot, so the payload is verified before it is trusted.
uint32_t wifiHintCrc(const WifiHint &h) {
  const uint8_t *p = (const uint8_t *)&h.ip;
  const size_t n = sizeof(WifiHint) - offsetof(WifiHint, ip);
  uint32_t v = 2166136261u;
  for (size_t i = 0; i < n; i++) {
    v ^= p[i];
    v *= 16777619u;
  }
  return v;
}

bool loadWifiHint(WifiHint &h) {
  if (!ESP.rtcUserMemoryRead(WIFI_HINT_RTC_SLOT, (uint32_t *)&h, sizeof(h))) {
    return false;
  }
  return h.magic == WIFI_HINT_MAGIC && h.crc == wifiHintCrc(h) &&
         h.channel >= 1 && h.channel <= 14 && h.ip != 0;
}

void clearWifiHint() {
  WifiHint h{};
  ESP.rtcUserMemoryWrite(WIFI_HINT_RTC_SLOT, (uint32_t *)&h, sizeof(h));
}

// The transmit power in use, reported by /api/info.
float g_wifiTxDbm = wifi_policy::TX_POWER_MAX_DBM;

// Radio settings, applied before an association. The SDK keeps the PHY mode
// in its own flash config, the output power only in RAM - so both are set
// on every boot, from the settings and the last signal strength.
void applyRadioSettings(const String &power, const String &mode, int8_t lastRssi) {
  WiFi.setPhyMode(mode == "b" ? WIFI_PHY_MODE_11B : (mode == "g" ? WIFI_PHY_MODE_11G : WIFI_PHY_MODE_11N));
  g_wifiTxDbm = wifi_policy::txPowerDbm(power.c_str(), lastRssi);
  WiFi.setOutputPower(g_wifiTxDbm);
  D_print(F("WiFi radio: 802.11"));
  D_print(mode == "b" ? "b" : (mode == "g" ? "g" : "n"));
  D_print(F(", "));
  D_print(g_wifiTxDbm);
  D_println(F(" dBm"));
}
// Back to full power - for a retry after a failed connect on "auto".
void wifiFullPower() {
  if (g_wifiTxDbm != wifi_policy::TX_POWER_MAX_DBM) {
    g_wifiTxDbm = wifi_policy::TX_POWER_MAX_DBM;
    WiFi.setOutputPower(g_wifiTxDbm);
    D_println(F("WiFi radio: back to full power"));
  }
}

// Remember what worked, so the next wake can skip the scan and DHCP.
// wakesSinceDhcp carries the lease-renewal counter forward; it defaults to 0,
// which is right for every caller that has just been through DHCP.
void saveWifiHint(uint8_t wakesSinceDhcp = 0) {
  WifiHint h{};
  h.magic = WIFI_HINT_MAGIC;
  h.wakesSinceDhcp = wakesSinceDhcp;
  h.ip = (uint32_t)WiFi.localIP();
  h.gw = (uint32_t)WiFi.gatewayIP();
  h.mask = (uint32_t)WiFi.subnetMask();
  h.dns = (uint32_t)WiFi.dnsIP();
  h.channel = WiFi.channel();
  const long rssi = WiFi.RSSI();
  h.rssi = (int8_t)(rssi < -127 ? -127 : (rssi > 0 ? 0 : rssi));
  const uint8_t *bssid = WiFi.BSSID();
  if (bssid != nullptr) {
    memcpy(h.bssid, bssid, sizeof(h.bssid));
  }
  h.crc = wifiHintCrc(h);
  ESP.rtcUserMemoryWrite(WIFI_HINT_RTC_SLOT, (uint32_t *)&h, sizeof(h));
}

