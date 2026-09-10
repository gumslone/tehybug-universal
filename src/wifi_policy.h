#pragma once
#include <stdint.h>
#include <string.h>

// Policy for the RTC-cached WiFi hint (wifi_service.h). Kept free of hardware
// so it can be unit-tested on the host (tests/test_wifi_policy.cpp).
namespace wifi_policy {

// Reusing the cached address skips the DHCP exchange — which also means the
// lease is never renewed, and that is true at every wake interval, not just
// long ones. A device waking every 10 s does not renew either; it just keeps
// the address visibly in use, so ARP and the router's MAC stickiness usually
// cover for it. Left alone long enough the router expires the lease and can
// hand the address to another device, and nothing on the fast path would
// notice: the association still succeeds, so it reports success while the
// device sits on a contested address and its sends quietly fail.
//
// So every Nth wake takes its address from DHCP instead, which renews the
// lease and re-caches the result. The scan is still skipped, and that is the
// larger saving (~1-2 s, against ~100 ms for DHCP), so this costs very little.
// 24 wakes is one renewal a day on an hourly schedule, and every four minutes
// on a 10 s one.
constexpr uint8_t DHCP_REFRESH_WAKES = 24;

// Whether this wake should take its address from DHCP rather than the cache.
inline bool renewLease(uint8_t wakesSinceDhcp,
                       uint8_t every = DHCP_REFRESH_WAKES) {
  if (every == 0) {
    return true; // "never reuse the cached address"
  }
  return (uint16_t)wakesSinceDhcp + 1 >= (uint16_t)every;
}

// The counter to store for the next wake: cleared whenever DHCP just ran,
// otherwise one higher. Saturates rather than wrapping, so a counter that
// somehow overshoots cannot roll back to zero and skip a whole renewal cycle.
inline uint8_t nextWakeCount(uint8_t wakesSinceDhcp, bool renewed) {
  if (renewed) {
    return 0;
  }
  return wakesSinceDhcp == 255 ? 255 : (uint8_t)(wakesSinceDhcp + 1);
}

// Transmit power for a connection, from the setting and the signal strength
// seen last time (0 = no reading yet). "max" is the chip's default and its
// best reach; "low" is for a device next to the router or on a supply that
// cannot deliver the transmit bursts; "auto" steps the power down while the
// signal is strong - the bursts are what dips a tired battery.
constexpr float TX_POWER_MAX_DBM = 20.5f;
constexpr float TX_POWER_LOW_DBM = 10.0f;
inline float txPowerDbm(const char *setting, int8_t lastRssi) {
  if (setting != nullptr && strcmp(setting, "low") == 0) {
    return TX_POWER_LOW_DBM;
  }
  if (setting != nullptr && strcmp(setting, "auto") == 0 && lastRssi != 0) {
    if (lastRssi >= -55) return TX_POWER_LOW_DBM;
    if (lastRssi >= -63) return 14.0f;
    if (lastRssi >= -70) return 17.0f;
  }
  return TX_POWER_MAX_DBM;
}

} // namespace wifi_policy
