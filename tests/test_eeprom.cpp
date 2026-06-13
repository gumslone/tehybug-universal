// Native host tests for the EEPROM data log (eeprom.h + EepromFS) running
// against a fake 32 KB I2C EEPROM (tests/shims/Wire.h). Covers the slot-32
// date index and the month-rollover behaviour that the index enables.
//
// Build & run:  tests/run.sh   (or see the g++ line there)
#include <cstdio>
#include <string>

// eeprom.h only forward-declares RtcTime and never calls it; a stub type is
// enough to satisfy the reference the constructor takes.
class RtcTime {};

#include "../eeprom.h"

// ---- tiny assertion framework -------------------------------------------
static int g_pass = 0, g_fail = 0;
static const char *g_case = "";
#define CASE(name) g_case = name
#define CHECK(cond)                                                      \
  do {                                                                   \
    if (cond) {                                                          \
      g_pass++;                                                          \
    } else {                                                             \
      g_fail++;                                                          \
      std::printf("  FAIL [%s] %s:%d: %s\n", g_case, __FILE__, __LINE__, \
                  #cond);                                                \
    }                                                                    \
  } while (0)
#define CHECK_EQ_STR(a, b)                                                  \
  do {                                                                      \
    std::string _a((a)), _b((b));                                          \
    if (_a == _b) {                                                         \
      g_pass++;                                                             \
    } else {                                                                \
      g_fail++;                                                             \
      std::printf("  FAIL [%s] %s:%d: \"%s\" != \"%s\"\n", g_case,          \
                  __FILE__, __LINE__, _a.c_str(), _b.c_str());             \
    }                                                                       \
  } while (0)

static RtcTime g_rtc;

// fresh, formatted filesystem on a wiped chip
static TeHyBugEeprom freshFs() {
  Wire.wipe();
  TeHyBugEeprom fs(g_rtc);
  fs.setup(); // formats the blank chip with SLOTS slots, then mounts
  return fs;
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

int main() {
  std::printf("Running eeprom tests...\n");
  test_format_and_capacity();
  test_append_and_read();
  test_date_index_roundtrip();
  test_index_hidden_and_dated_in_listing();
  test_month_rollover();
  test_recycle_preserves_index();
  std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
  return g_fail == 0 ? 0 : 1;
}
