// Native host tests for the EEPROM data log (eeprom.h + EepromFS) running
// against a fake 32 KB I2C EEPROM (tests/shims/Wire.h). Covers the slot-32
// date index and the month-rollover behaviour that the index enables.
//
// Build & run:  tests/run.sh   (or see the g++ line there)
#include "test_framework.h"
#include <cstring>  // std::strstr

// eeprom.h only forward-declares RtcTime and never calls it; a stub type is
// enough to satisfy the reference the constructor takes.
class RtcTime {};

#include "../src/eeprom.h"

static RtcTime g_rtc;

// fresh, formatted filesystem on a wiped chip
static TeHyBugEeprom freshFs() {
  Wire.wipe();
  TeHyBugEeprom fs(g_rtc);
  fs.setup(); // formats the blank chip with SLOTS slots, then mounts
  return fs;
}

// fresh, formatted filesystem on a wiped chip of a given capacity
static TeHyBugEeprom freshFsAt(size_t capacity) {
  Wire.wipe(capacity);
  TeHyBugEeprom fs(g_rtc);
  fs.setup();
  return fs;
}

// one byte straight to the simulated chip, bypassing EepromFS entirely
static void pokeRaw(unsigned int addr, uint8_t value) {
  Wire.beginTransmission(0x50);
  Wire.write((int)(addr / 256));
  Wire.write((int)(addr % 256));
  Wire.write((int)value);
  Wire.endTransmission();
}

static uint8_t peekRaw(unsigned int addr) {
  Wire.beginTransmission(0x50);
  Wire.write((int)(addr / 256));
  Wire.write((int)(addr % 256));
  Wire.endTransmission();
  Wire.requestFrom(0x50, 1);
  return (uint8_t)Wire.read();
}

// largest a day file grows to before the slot fills and it wraps — i.e. the
// usable size of one slot, which is what the chip capacity decides
static unsigned int maxDayFileSize(TeHyBugEeprom &fs) {
  unsigned int best = 0;
  for (int i = 0; i < 400; i++) {
    char line[24];
    std::snprintf(line, sizeof(line), "L%03d 22.6t\n", i);
    fs.appendLine("9.txt", line, 9);
    const unsigned int len = fs.read("9.txt").length();
    if (len > best) best = len;
  }
  return best;
}

static void test_detects_chip_capacity() {
  CASE("chip capacity decides the day-file size");
  // FT24C256A: (32768-4)/32 = 1023 per slot, less the 16-byte file header
  TeHyBugEeprom small = freshFsAt(FakeWire::SIZE_32K);
  const unsigned int smallMax = maxDayFileSize(small);
  CHECK(smallMax > 900 && smallMax <= 1007);

  // FT24C512A: (65536-4)/32 = 2047 per slot, so roughly twice as much. The
  // detection used to answer "32 KB" for this part, leaving half the chip idle.
  TeHyBugEeprom big = freshFsAt(FakeWire::SIZE_64K);
  const unsigned int bigMax = maxDayFileSize(big);
  CHECK(bigMax > 1900 && bigMax <= 2031);
  CHECK(bigMax > smallMax + 900); // the extra capacity is really being used
}

static void test_capacity_change_reformats_once() {
  CASE("a bigger chip re-lays out the log, but only once");
  TeHyBugEeprom fs = freshFsAt(FakeWire::SIZE_32K);
  CHECK(fs.appendLine("13.txt", "07:55 22.6t\n", 13));
  CHECK_EQ_STR(fs.read("13.txt").c_str(), "07:55 22.6t\n"); // also flushes

  // same stored bytes, but the part now reports 64 KB: the slot size doubles,
  // so every slot offset moves and the old layout cannot be read back
  Wire.setCapacity(FakeWire::SIZE_64K);
  TeHyBugEeprom after(g_rtc);
  after.setup();
  CHECK(after.mounted());
  CHECK_EQ_STR(after.read("13.txt").c_str(), ""); // reformatted, log cleared
  CHECK(after.appendLine("13.txt", "08:00 22.7t\n", 13));
  CHECK_EQ_STR(after.read("13.txt").c_str(), "08:00 22.7t\n");

  // The reformat must not repeat: format() zeroes the header, so without
  // re-stamping the capacity marker every boot would wipe the log again.
  TeHyBugEeprom third(g_rtc);
  third.setup();
  CHECK(third.mounted());
  CHECK_EQ_STR(third.read("13.txt").c_str(), "08:00 22.7t\n");
}

static void test_legacy_filesystem_keeps_its_data() {
  CASE("a pre-marker 32 KB filesystem is adopted, not wiped");
  TeHyBugEeprom fs = freshFsAt(FakeWire::SIZE_32K);
  CHECK(fs.appendLine("13.txt", "07:55 22.6t\n", 13));
  CHECK_EQ_STR(fs.read("13.txt").c_str(), "07:55 22.6t\n");

  // firmware from before the capacity marker existed left header byte 2 at 0.
  // A 32 KB part lays out identically then and now, so the data must survive.
  pokeRaw(2, 0);
  CHECK(peekRaw(2) == 0);

  TeHyBugEeprom after(g_rtc);
  after.setup();
  CHECK(after.mounted());
  CHECK_EQ_STR(after.read("13.txt").c_str(), "07:55 22.6t\n");
  CHECK(peekRaw(2) == 2); // marker adopted, so this check is not repeated
}

static void test_factory_reset_from_unmounted() {
  CASE("factory reset erases the log without a prior mount");
  {
    TeHyBugEeprom fs = freshFsAt(FakeWire::SIZE_64K);
    CHECK(fs.appendLine("13.txt", "07:55 22.6t\n", 13));
    CHECK(fs.appendLine("14.txt", "08:55 21.0t\n", 14));
    fs.setFileDate(13, "2026-07-13");
    CHECK_EQ_STR(fs.read("13.txt").c_str(), "07:55 22.6t\n"); // also flushes
  }

  // handleFactoryReset() formats from a fresh object: on the 20 s button-hold
  // path setupSensors() has not mounted the EEPROM, so the capacity is still
  // unknown and format() has to detect it before it can size the slots.
  TeHyBugEeprom reset(g_rtc);
  reset.format();
  CHECK(reset.mounted());
  CHECK_EQ_STR(reset.read("13.txt").c_str(), "");
  CHECK_EQ_STR(reset.read("14.txt").c_str(), "");
  CHECK_EQ_STR(reset.fileDate(13).c_str(), "");
  const std::string listing(reset.listFilesJson().c_str());
  CHECK(listing.find("13.txt") == std::string::npos);
  CHECK(listing.find("14.txt") == std::string::npos);

  // the wiped filesystem is usable straight away, and stays wiped across a
  // reboot rather than being reformatted again by the capacity check
  CHECK(reset.appendLine("13.txt", "09:00 20.0t\n", 13));
  TeHyBugEeprom after(g_rtc);
  after.setup();
  CHECK(after.mounted());
  CHECK_EQ_STR(after.read("13.txt").c_str(), "09:00 20.0t\n");
}

static void test_dead_bus_never_formats() {
  CASE("a silent bus never triggers a format");
  // healthy chip with data
  TeHyBugEeprom fs = freshFsAt(FakeWire::SIZE_32K);
  CHECK(fs.appendLine("13.txt", "07:55 22.6t\n", 13));
  CHECK_EQ_STR(fs.read("13.txt").c_str(), "07:55 22.6t\n");

  // the chip stops answering (transient bus failure at mount time): setup()
  // must not read that as "blank chip" and format - the hardware incident
  // this pins showed "EEPROM unformatted, formatting..." over an intact log
  Wire.setPresent({});
  TeHyBugEeprom during(g_rtc);
  during.setup();
  CHECK(!during.mounted()); // no log this wake, but nothing touched

  // bus comes back: the data must still be there
  Wire.setPresent({0x50});
  TeHyBugEeprom after(g_rtc);
  after.setup();
  CHECK(after.mounted());
  CHECK_EQ_STR(after.read("13.txt").c_str(), "07:55 22.6t\n");
}

static void test_format_and_capacity() {
  CASE("format/capacity");
  TeHyBugEeprom fs = freshFs();
  CHECK(fs.mounted());
  // 32 KB / 32 slots, minus headers -> ~1 KB usable per day file
  CHECK(EFS_FILEHEADERSIZE == 16);
  CHECK(TeHyBugEeprom::SLOTS == 32);
}

static void test_append_and_read() {
  CASE("append/read");
  TeHyBugEeprom fs = freshFs();
  CHECK(fs.appendLine("13.txt", "07:55 22.6t 48.3h\n", 13));
  CHECK(fs.appendLine("13.txt", "07:56 22.7t 48.2h\n", 13));
  CHECK_EQ_STR(fs.read("13.txt").c_str(),
               "07:55 22.6t 48.3h\n07:56 22.7t 48.2h\n");
}

static void test_date_index_roundtrip() {
  CASE("date index round-trip");
  TeHyBugEeprom fs = freshFs();
  fs.setFileDate(13, "2026-06-13");
  fs.setFileDate(1, "2026-06-01");
  // day 1 and day 13 must not collide on a prefix match
  CHECK_EQ_STR(fs.fileDate(13).c_str(), "2026-06-13");
  CHECK_EQ_STR(fs.fileDate(1).c_str(), "2026-06-01");
  CHECK_EQ_STR(fs.fileDate(7).c_str(), ""); // unknown day -> empty
  // replacing a day's date overwrites, does not duplicate
  fs.setFileDate(13, "2026-07-13");
  CHECK_EQ_STR(fs.fileDate(13).c_str(), "2026-07-13");
  CHECK_EQ_STR(fs.fileDate(1).c_str(), "2026-06-01"); // others untouched
}

static void test_index_hidden_and_dated_in_listing() {
  CASE("listing hides index, tags dates");
  TeHyBugEeprom fs = freshFs();
  fs.appendLine("13.txt", "x\n", 13);
  fs.setFileDate(13, "2026-06-13");
  const std::string json = fs.read(TeHyBugEeprom::INDEX_FILE).length()
                               ? std::string(fs.listFilesJson().c_str())
                               : "";
  // the index slot itself is not listed as a day file
  CHECK(json.find("\"idx\"") == std::string::npos);
  // the day file is listed with its calendar date
  CHECK(json.find("\"name\":\"13.txt\"") != std::string::npos);
  CHECK(json.find("\"date\":\"2026-06-13\"") != std::string::npos);
}

static void test_month_rollover() {
  CASE("month rollover clears stale slot");
  TeHyBugEeprom fs = freshFs();
  // June 13: write two readings and record the date
  fs.setFileDate(13, "2026-06-13");
  fs.appendLine("13.txt", "07:55 20.0t\n", 13);
  fs.appendLine("13.txt", "07:56 20.1t\n", 13);
  CHECK(fs.read("13.txt").length() > 0);

  // July 13: the slot's stored date differs -> this is what logSensorData
  // checks. Reset the file and record the new date, then append fresh.
  CHECK(fs.fileDate(13) != String("2026-07-13"));
  fs.resetDayFile("13.txt", 13);
  fs.setFileDate(13, "2026-07-13");
  fs.appendLine("13.txt", "09:00 25.0t\n", 13);

  // file now holds only July data, index reflects July
  CHECK_EQ_STR(fs.read("13.txt").c_str(), "09:00 25.0t\n");
  CHECK_EQ_STR(fs.fileDate(13).c_str(), "2026-07-13");
}

static void test_recycle_preserves_index() {
  CASE("recycle never drops the index slot");
  TeHyBugEeprom fs = freshFs();
  fs.setFileDate(20, "2026-06-20"); // creates the index slot
  // fill the remaining slots with day files until the FS is full
  for (int d = 1; d <= 40; d++) {
    char name[16];
    std::snprintf(name, sizeof(name), "%d.txt", d);
    fs.appendLine(name, "data\n", (uint8_t)(d > 31 ? 15 : d));
  }
  // forcing new files past capacity must recycle day files, not the index
  CHECK_EQ_STR(fs.fileDate(20).c_str(), "2026-06-20");
}

static void test_read_nonexistent() {
  CASE("read of a missing file is empty");
  TeHyBugEeprom fs = freshFs();
  CHECK_EQ_STR(fs.read("nope.txt").c_str(), "");
  CHECK_EQ_STR(fs.fileDate(9).c_str(), ""); // no index yet
}

static void test_slot_full_wraps() {
  CASE("a full day file wraps to the start");
  TeHyBugEeprom fs = freshFs();
  // Fill the ~1 KB slot with many small lines. Once full, appending must keep
  // succeeding by wrapping: the file is cleared and the new line written at the
  // top, so the stored size drops back down instead of logging stopping.
  bool wrapped = false;
  unsigned int prevLen = 0;
  for (int i = 0; i < 300; i++) {
    char line[24];
    std::snprintf(line, sizeof(line), "L%03d 22.6t\n", i);
    CHECK(fs.appendLine("9.txt", line, 9)); // never rejected
    const unsigned int len = fs.read("9.txt").length();
    if (len < prevLen) wrapped = true;      // size shrank => wrapped to start
    prevLen = len;
  }
  CHECK(wrapped);                            // it wrapped at least once
  CHECK(fs.read("9.txt").length() < 1100);   // still within one slot
  // the most recent line survived the wrap
  CHECK(std::strstr(fs.read("9.txt").c_str(), "L299") != nullptr);
}

static void test_recycle_picks_oldest_by_wrap() {
  CASE("recycle picks the oldest day across the month wrap");
  TeHyBugEeprom fs = freshFs();
  fs.setFileDate(10, "2026-06-10"); // index occupies one slot
  // fill the remaining 31 slots with day files 1..31
  for (int d = 1; d <= 31; d++) {
    char name[16];
    std::snprintf(name, sizeof(name), "%d.txt", d);
    fs.appendLine(name, "x\n", (uint8_t)d);
  }
  // FS is full; appending on day 2 must recycle the file furthest in the
  // past, which for currentMday=2 is day 3 ((2-3+31)%31 = 30, the max age)
  fs.appendLine("99.txt", "new\n", 2);
  CHECK_EQ_STR(fs.read("3.txt").c_str(), "");      // day 3 recycled
  CHECK_EQ_STR(fs.read("99.txt").c_str(), "new\n"); // new file written
  CHECK_EQ_STR(fs.read("1.txt").c_str(), "x\n");   // newer days kept
  CHECK_EQ_STR(fs.fileDate(10).c_str(), "2026-06-10"); // index untouched
}

static void test_hourly_keeps_a_full_day() {
  CASE("hourly mode keeps all 24 hours without recycling");
  TeHyBugEeprom fs = freshFs();
  fs.setSlotWrap(24); // hour-of-day slots, not day-of-month
  fs.setFileDate(10, "2026-06-14 10");

  // A full day is 24 hour files plus the index — 25 of the 32 slots — so a
  // complete 24 h of history has to survive with nothing recycled.
  for (int h = 0; h <= 23; h++) {
    char name[16];
    std::snprintf(name, sizeof(name), "%d.txt", h);
    char line[16];
    std::snprintf(line, sizeof(line), "h%d\n", h);
    CHECK(fs.appendLine(name, line, (uint8_t)h));
  }
  for (int h = 0; h <= 23; h++) {
    char name[16], want[16];
    std::snprintf(name, sizeof(name), "%d.txt", h);
    std::snprintf(want, sizeof(want), "h%d\n", h);
    CHECK_EQ_STR(fs.read(name).c_str(), want);
  }
  CHECK_EQ_STR(fs.fileDate(10).c_str(), "2026-06-14 10"); // index untouched
}

static void test_slot_wrap_recycles_by_period() {
  CASE("recycling measures age with the configured slot wrap");
  // Month mode (wrap 31) must still pick the day furthest in the past even
  // when a stale hour-numbered file is present; with a fixed wrap the distance
  // for an out-of-range slot went negative and that file was always chosen.
  TeHyBugEeprom fs = freshFs();
  fs.setSlotWrap(31);
  fs.setFileDate(10, "2026-06-10");
  for (int d = 1; d <= 31; d++) {
    char name[16];
    std::snprintf(name, sizeof(name), "%d.txt", d);
    fs.appendLine(name, "x\n", (uint8_t)d);
  }
  fs.appendLine("99.txt", "new\n", 2);
  CHECK_EQ_STR(fs.read("3.txt").c_str(), "");       // day 3 is the oldest
  CHECK_EQ_STR(fs.read("99.txt").c_str(), "new\n");
  CHECK_EQ_STR(fs.fileDate(10).c_str(), "2026-06-10");
}

int main() {
  std::printf("Running eeprom tests...\n");
  test_dead_bus_never_formats();
  test_detects_chip_capacity();
  test_capacity_change_reformats_once();
  test_legacy_filesystem_keeps_its_data();
  test_factory_reset_from_unmounted();
  test_format_and_capacity();
  test_append_and_read();
  test_date_index_roundtrip();
  test_index_hidden_and_dated_in_listing();
  test_month_rollover();
  test_recycle_preserves_index();
  test_read_nonexistent();
  test_slot_full_wraps();
  test_recycle_picks_oldest_by_wrap();
  test_hourly_keeps_a_full_day();
  test_slot_wrap_recycles_by_period();
  return SUMMARY();
}
