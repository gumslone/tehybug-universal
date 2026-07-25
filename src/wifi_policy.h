#pragma once
#include <stdint.h>

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

} // namespace wifi_policy
