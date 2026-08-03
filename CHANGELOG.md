# Changelog

Release notes, newest first. A version section is appended automatically by the
build on every release (see [`ci/changelog.sh`](ci/changelog.sh)). Firmware
versions are date-based (`YYMMDDHHMM`); see the
[releases](https://github.com/gumslone/tehybug-universal/releases) for the tags.

## v2608031837 (2026-08-03)

- Reach the portal when the saved network is gone; sleep without crashing

## v2608021236 (2026-08-02)

- Serve the full config to the UI; fill links and a cause on the Data Log page
- Explain the port trade-offs on the sensor settings page
- Give the second temperature sensor its own query parameter
- Revalidate the JS bundle on every load instead of caching it for 10 min
- Offer the sensor suggestions in metric or imperial
- Suggestion links below the fields, never disabled
- Let the portal build payloads and query strings from the device's sensors
- Never format the data log over a bus that is not answering
- Drop the light-sleep AP pinning - the bench proved it slower
- Retire the Lua firmware's HA entities; average the ADC like it did
- Run the test workflow on the restructure branch too
- Probe both I2C orientations and settle a freshly grounded DHT
- Bring the README's button and mDNS instructions up to date
- Move the boot precedence rules into mode_logic
- Slim the sketch to a composition root
- Leave mDNS out of the generic build - 23 KB back, headroom 6x
- Judge the forced-light-sleep chunks in mode_logic, execute in sleep_modes
- Decide the serve pass in mode_logic, execute it in data_service
- Move scenario conditions and the compact log line into the pure layer
- Make every module self-sufficient; stop depending on include order
- Revert "Measure the real sleep duration to catch a reset on the first press"
- Measure the real sleep duration to catch a reset on the first press
- Tell a reset button from a timer wake with an RTC boot mark
- Only drain MQTT before sleep when there was a link to drain into
- Drop the pre-sleep settle for HTTP; give MQTT a bounded drain instead
- Never publish a truncated Home Assistant discovery document
- Re-associate light sleep to the cached AP instead of scanning for it
- Keep release builds quiet, and log the mode the device actually entered
- Stop writing unresolved placeholders into the EEPROM data log
- Name the DHT pins in board.h, back on the original mapping
- Confirm the DHT model instead of reading a timeout as DHT11
- Detect the DHT model, and power the sensor before probing it
- Fix the I2C pin mapping on the generic build - SDA and SCL were crossed
- Leave the LED pin alone in a generic debug build - it is the TX line
- Rebuild firmware binaries
- Draw the config-mode page before anything from the internet
- Read the AHT20 on the generic build, and say when a DHT read fails
- Let the SDK settle before asking it for a forced light sleep
- Keep light sleep asleep for the interval it was asked for
- Log what a light sleep actually cost, and whether the SDK refused it
- Stop paying for a button window and a radio nobody uses on wake
- Renew the DHCP lease periodically on the WiFi fast path
- Report the detected EEPROM capacity so it can be verified
- Make the factory reset actually clear the data log
- Detect the FT24C512A so the data log uses the whole 64 KB chip

## v2607251755 (2026-07-25)

- Stop the Data Serving page silently switching Home Assistant off
- Namespace the plain-MQTT availability topic
- Move the wake-interval decision into the tested pure layer
- Restore the WiFi fast reconnect and the HA discovery memo
- Remove the RTC-backed WiFi fast reconnect and HA discovery memo
- Make the wake interval a shared helper and the HA refresh time-based
- Let the SDK's own auto-connect finish before taking over the WiFi
- Never re-associate a WiFi link that is already up
- Retry the MQTT connect within a budget on sleep wakes
- Only reuse the cached address when it is complete, and report the DNS
- Cache the WiFi channel, BSSID and IP in RTC memory for a fast reconnect
- Skip redundant HA discovery on deep-sleep wakes, and retire removed sensors
- Only subscribe to the MQTT control topics when plain MQTT is active
- Stop dropping out of config mode when WiFi fails to connect
- Scan the I2C bus twice per boot, not three times
- Fix clang-tidy CI failure and silence the Node 20 deprecation warnings
- Drop the orange MODE-button window indicator
- Fix the pixel dropping its first frame after power-up
- Fix LED never driven off, lost boot logs, and a 3 KB config round-trip
- Store I2C scan results as a bitmask instead of a searched String
- Document the stay-connected vs reconnect threshold for light sleep
- Fix light sleep erasing the WiFi credentials; split it into its own mode
- Add a DeviceMode resolver; fix WiFi never reconnecting after light sleep
- Give the MODE button a real window in live mode, and show it on the LED
- Bump BugZapper submodule: non-blocking flash start, copyable log output
- Fix device key stuck on "Loading...", and exclusive modes on the cloud/HA pages
- Polish: HA metadata for the second sensor, safer logging and config writes
- Bound every blocking path in live mode; fix hourly recycling and POST URLs (wave 3)
- Reduce heap and flash-wear pressure (wave 2)
- Add firmware size guard, reclaim OTA headroom, fix changelog generation
- Fix silently-broken features and blocking/hang paths (wave 1)

## v2606151601 (2026-06-15)

- Maintenance and build updates

## v2606141708 (2026-06-14)

- Maintenance and build updates

## v2606140640 (2026-06-14)

- Maintenance and build updates

## Notable features

### Offline data logging

- Log readings to an attached RTC + EEPROM module with no server or network — one file per day of month, a full month retained, each tagged with its full calendar date.
- Pick exactly which values to log with placeholders (e.g. `%temp% %humi%`); a compact on-device format fits more entries.
- **Offline mode**: the device runs with WiFi off for the lowest power draw. Enabling it switches every other mode off.
- Configure and read the log on the **Data Log** page.

### Usability

- Inline help under every setting on the configuration pages.
- More reliable return to config mode from offline / deep-sleep modes after a reset.
- **Downloads & Changelog** page, with advice to update only when needed.

### Fixes

- Offline mode no longer falls back to WiFi when the EEPROM is present.
- A day file reused in a new month no longer mixes dates.
- The dashboard sensor table is no longer cleared right after connecting.
- The offline fallback page now shows the device IP address.
- Factory reset now also erases the on-device data log (RTC + EEPROM module).
- Saving the Data Log page no longer drops the device out of config mode.
- The blue LED now reliably indicates config mode on every boot.
