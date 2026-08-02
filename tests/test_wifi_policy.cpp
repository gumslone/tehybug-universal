// Native host tests for the DHCP lease-renewal policy behind the RTC-cached
// WiFi hint (src/wifi_policy.h).
#include "test_framework.h"
#include "../src/wifi_policy.h"

using namespace wifi_policy;

static void test_renew_lease_cadence() {
  CASE("renewLease fires once every N wakes");
  // a fresh count means DHCP just ran, so the cache is good for a while
  CHECK(!renewLease(0));
  CHECK(!renewLease(1));
  CHECK(!renewLease(DHCP_REFRESH_WAKES - 2));
  // the Nth wake since DHCP is the one that renews
  CHECK(renewLease(DHCP_REFRESH_WAKES - 1));
  // and anything beyond it still renews, so a counter that somehow ran past
  // the boundary cannot skip the renewal entirely
  CHECK(renewLease(DHCP_REFRESH_WAKES));
  CHECK(renewLease(255));
}

static void test_renew_lease_period_is_configurable() {
  CASE("renewLease honours the period");
  CHECK(renewLease(0, 1));   // every wake
  CHECK(!renewLease(0, 2));
  CHECK(renewLease(1, 2));   // every other wake
  // 0 means "never reuse the cached address", not "never renew" — getting this
  // backwards would silently pin a stale address forever
  CHECK(renewLease(0, 0));
  CHECK(renewLease(200, 0));
}

static void test_next_wake_count() {
  CASE("nextWakeCount advances and resets");
  CHECK(nextWakeCount(0, false) == 1);
  CHECK(nextWakeCount(5, false) == 6);
  // a renewing wake restarts the count, so the next renewal is a full period
  // away rather than every wake from here on
  CHECK(nextWakeCount(DHCP_REFRESH_WAKES - 1, true) == 0);
  CHECK(nextWakeCount(0, true) == 0);
  // saturates instead of wrapping: rolling 255 -> 0 would look like a fresh
  // lease and postpone the renewal by another full period
  CHECK(nextWakeCount(254, false) == 255);
  CHECK(nextWakeCount(255, false) == 255);
}

static void test_full_cycle_renews_and_repeats() {
  CASE("a run of wakes renews on schedule, repeatedly");
  uint8_t count = 0;
  int renewals = 0;
  int wakes = 0;
  for (int i = 0; i < DHCP_REFRESH_WAKES * 3; i++) {
    const bool renewing = renewLease(count);
    if (renewing) {
      renewals++;
    }
    count = nextWakeCount(count, renewing);
    wakes++;
  }
  // exactly one renewal per period, no drift and no run of back-to-back ones
  CHECK(wakes == DHCP_REFRESH_WAKES * 3);
  CHECK(renewals == 3);
  // a saturated counter still recovers to a normal cadence
  uint8_t stuck = 255;
  CHECK(renewLease(stuck));
  stuck = nextWakeCount(stuck, true);
  CHECK(stuck == 0);
  CHECK(!renewLease(stuck));
}

int main() {
  std::printf("Running wifi_policy tests...\n");
  test_renew_lease_cadence();
  test_renew_lease_period_is_configurable();
  test_next_wake_count();
  test_full_cycle_renews_and_repeats();
  return SUMMARY();
}
