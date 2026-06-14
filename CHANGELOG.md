# Changelog

Release notes, newest first. A version section is appended automatically by the
build on every release (see [`ci/changelog.sh`](ci/changelog.sh)). Firmware
versions are date-based (`YYMMDDHHMM`); see the
[releases](https://github.com/gumslone/tehybug-universal/releases) for the tags.

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
