#pragma once
#include "debug.h"

// eeprom.h only holds a RtcTime reference (never calls it), so a forward
// declaration is enough and keeps this header free of the rtc_time.h ->
// configuration.h chain — which lets it be unit-tested on the host (see
// tests/). In the firmware build rtc_time.h is fully included earlier.
class RtcTime;

#if defined(ARDUINO_ESP8266_GENERIC)
// The generic (1MB) build (old / first-gen TeHyBug) drops the data-log
// EEPROM driver to save flash; this stub keeps the call sites (data logging,
// /api/datalog) compiling. mounted() stays false, so the endpoints report
// "not active".
class TeHyBugEeprom {
  public:
    TeHyBugEeprom(RtcTime &) {}
    void setup() {}
    bool mounted() { return false; }
    void readdir() {}
    String read(const char *) { return String(); }
    bool appendLine(const String &, const String &, uint8_t) { return false; }
    String listFilesJson() { return "[]"; }
    String fileDate(uint8_t) { return String(); }
    void setFileDate(uint8_t, const String &) {}
    bool resetDayFile(const String &, uint8_t) { return false; }
    void setSlotWrap(uint8_t) {}
    void format() {}
};
#else
#include <EepromFS.h>

// Slot-based data log on the external I2C EEPROM on the DS3231 RTC module.
// Current modules carry an FT24C512A (64 KB); earlier ones an FT24C256A
// (32 KB). The capacity is detected at mount and decides the slot size, so a
// day file is ~2 KB on the newer modules and ~1 KB on the older ones.
//
// One file per day of month ("<mday>.txt"), so 32 slots hold a full month of
// daily logs; when no free slot is left the oldest day file is removed.
// NOTE: begin() reuses the slot count an already-formatted EEPROM was created
// with — SLOTS only applies when formatting a blank chip.
class TeHyBugEeprom{
  public :
  static constexpr uint8_t SLOTS = 32;
  // Day files use at most 31 slots (one per day of month). The 32nd slot
  // holds this index file, which maps each day file to the full calendar
  // date it currently stores — the file names ("<mday>.txt") only carry the
  // day of month, so this is what tells June 13th apart from July 13th.
  static constexpr const char *INDEX_FILE = "idx";

  TeHyBugEeprom(RtcTime & time): m_efs(0x50, 0), m_time(time) {}

  void setup(){
      uint8_t slots = m_efs.begin();
      const uint8_t want = sizeCode(m_efs.esize());
      if (slots > 0 && !layoutMatches(want)) {
        // The slot layout is derived from the chip capacity, so a filesystem
        // written for a different one puts every slot at the wrong offset.
        D_println("EEPROM capacity changed, reformatting...");
        slots = 0;
      }
      if (slots == 0) {
        // unformatted eeprom: create the filesystem once
        D_println("EEPROM unformatted, formatting...");
        m_efs.format(SLOTS);
        slots = m_efs.begin();
        // only stamp a filesystem that actually mounted: with no EEPROM on the
        // bus the detection failed and every write here would just NACK
        if (slots > 0) {
          markSize(sizeCode(m_efs.esize()));
        }
      }
      m_mounted = slots > 0;
      D_print("EEPROM filesystem mounted, slots: ");
      D_println(slots);
      readdir();
    }

  bool mounted() {
    return m_mounted;
  }

  // Tell the log how slot numbers wrap so recycling picks the right "oldest"
  // file: 31 for day-of-month slots, 24 for hour-of-day slots.
  void setSlotWrap(uint8_t wrap) {
    m_slotWrap = (wrap > 0) ? wrap : 31;
  }

  // Erase all logged data by re-creating the filesystem (used by factory
  // reset). Re-mounts afterwards so logging can resume without a reboot.
  void format() {
    D_println("Formatting EEPROM data log...");
    m_efs.format(SLOTS);
    m_mounted = m_efs.begin() > 0;
    // format() zeroes the header, so the capacity marker has to be re-stamped
    // or the next boot would read "laid out for an unknown chip" and reformat
    // again — wiping the log on every single boot.
    markSize(sizeCode(m_efs.esize()));
  }

  void readdir() {
    m_efs.dirp=0;
    while (uint8_t f=m_efs.readdir()) {
            D_print("Slot ");
            D_print(f);
            D_print(": ");
            D_print(m_efs.filename(f));
            D_print(" ");
            D_println(m_efs.filesize(f));
     }

     D_print("Error status: "); D_println(m_efs.ferror);
  }

  String read(const char *n) {
    String data;
    const uint8_t f = m_efs.fopen(n, "r");
    if (!f) {
      D_println("Read error");
      return data;
    }
    data.reserve(m_efs.filesize(f));
    // Each fgetc is its own I2C transaction, so a ~1 KB slot is ~1000 of them.
    // This runs from the /api/datalog HTTP handler, so yield periodically to
    // keep the WiFi stack serviced and the watchdog fed.
    unsigned int read = 0;
    while (!m_efs.eof(f)) {
      data += (char)m_efs.fgetc(f);
      if ((++read & 0x3F) == 0) { // every 64 bytes
        yield();
      }
    }
    m_efs.fclose(f);
    return data;
  }

  // appends a line to the day file, dropping the oldest file when the
  // filesystem is full. When the day file itself fills up it wraps: the file is
  // cleared and the line is written at the start, so logging keeps going
  // (overwriting the day's earlier entries) instead of stopping. Returns false
  // only when nothing could be written at all.
  bool appendLine(const String & name, const String & line, uint8_t currentMday) {
    if (!m_mounted) {
      return false;
    }
    uint8_t f = m_efs.fopen(name.c_str(), "a");
    if (!f) {
      // no free slot for a new day: recycle the oldest day file
      removeOldestFile(currentMday);
      f = m_efs.fopen(name.c_str(), "a");
    }
    if (!f) {
      D_println("EEPROM append error");
      return false;
    }
    bool full = false;
    for (size_t i = 0; i < line.length(); i++) {
      if (!m_efs.fputc(line[i], f)) {
        full = true; // slot is full mid-write
        break;
      }
    }
    m_efs.fflush(f);
    // debug-only: watch the day file grow toward the slot limit (~1006 B)
    D_print("EEPROM log ");
    D_print(name);
    D_print(": ");
    D_print(m_efs.filesize(f));
    D_println(full ? " B (FULL)" : " B");
    m_efs.fclose(f);

    if (full) {
      // Day file full: wrap to the start. Clear it and write this line at the
      // top (truncating discards the partial write above too).
      D_println("Day file full, wrapping to start");
      return writeFile(name.c_str(), line);
    }
    return true;
  }

  // JSON array of the stored day files, each tagged with its calendar date
  // from the index slot: [{"name":"10.txt","size":123,"date":"2026-06-10"},...]
  String listFilesJson() {
    String json = "[";
    if (m_mounted) {
      const String idx = read(INDEX_FILE); // read the date index once
      m_efs.dirp = 0;
      bool first = true;
      while (uint8_t f = m_efs.readdir()) {
        const char *name = m_efs.filename(f);
        if (strcmp(name, INDEX_FILE) == 0) {
          continue; // the index slot itself is metadata, not a day file
        }
        if (!first) {
          json += ",";
        }
        first = false;
        json += "{\"name\":\"" + String(name) + "\",\"size\":" +
                String(m_efs.filesize(f)) + ",\"date\":\"" +
                dateFromIndex(idx, atoi(name)) + "\"}";
      }
    }
    json += "]";
    return json;
  }

  // the calendar date currently stored in the day file for `mday`, or ""
  String fileDate(uint8_t mday) {
    return dateFromIndex(read(INDEX_FILE), mday);
  }

  // record `date` (YYYY-MM-DD) as the day file's date in the index slot,
  // replacing any previous entry for `mday`
  void setFileDate(uint8_t mday, const String &date) {
    if (!m_mounted) {
      return;
    }
    const String key = String((int)mday) + " ";
    const String idx = read(INDEX_FILE);
    String out;
    int pos = 0;
    while (pos < (int)idx.length()) {
      int eol = idx.indexOf('\n', pos);
      if (eol < 0) {
        eol = idx.length();
      }
      const String entry = idx.substring(pos, eol);
      if (entry.length() > 0 && !entry.startsWith(key)) {
        out += entry + "\n";
      }
      pos = eol + 1;
    }
    out += key + date + "\n";
    writeFile(INDEX_FILE, out);
  }

  // truncate the day file to empty (recycling the oldest day file if the FS
  // is full), used when a day slot is reused for a new calendar date
  bool resetDayFile(const String &name, uint8_t mday) {
    if (!m_mounted) {
      return false;
    }
    uint8_t f = m_efs.fopen(name.c_str(), "w");
    if (!f) {
      removeOldestFile(mday);
      f = m_efs.fopen(name.c_str(), "w");
    }
    if (!f) {
      return false;
    }
    m_efs.fclose(f); // "w" + immediate close leaves a 0-byte file
    return true;
  }

  private:

  // extract the "YYYY-MM-DD" stored for `mday` from index content made of
  // "<mday> <date>" lines; returns "" when the day has no entry
  String dateFromIndex(const String &idx, int mday) {
    const String key = String(mday) + " ";
    int pos = 0;
    while (pos < (int)idx.length()) {
      int eol = idx.indexOf('\n', pos);
      if (eol < 0) {
        eol = idx.length();
      }
      const String entry = idx.substring(pos, eol);
      if (entry.startsWith(key)) {
        return entry.substring(key.length());
      }
      pos = eol + 1;
    }
    return String();
  }

  // overwrite a whole file (fopen "w" truncates to the bytes written);
  // returns true when all content was written
  bool writeFile(const char *name, const String &content) {
    uint8_t f = m_efs.fopen(name, "w");
    if (!f) {
      return false; // the index slot is excluded from recycling, so this is rare
    }
    bool ok = true;
    for (size_t i = 0; i < content.length(); i++) {
      if (!m_efs.fputc(content[i], f)) {
        ok = false;
        break;
      }
    }
    m_efs.fflush(f);
    m_efs.fclose(f);
    return ok;
  }

  // removes the day file furthest in the past relative to currentMday
  // `current` is the slot in use now: a day of month (1-31) in month mode, or
  // an hour of day (0-23) in hourly mode. Distances are measured modulo
  // m_slotWrap so the oldest slot is picked correctly in both — with the wrap
  // hardcoded to 31 this chose the wrong file for hourly logs.
  void removeOldestFile(uint8_t current) {
    uint8_t oldestAge = 0;
    char oldestName[EFS_FILENAMELENGTH + 1] = {0};

    m_efs.dirp = 0;
    while (uint8_t f = m_efs.readdir()) {
      if (strcmp(m_efs.filename(f), INDEX_FILE) == 0) {
        continue; // never recycle the date index slot
      }
      const int fileSlot = atoi(m_efs.filename(f));
      // Positive modulo: a slot number outside the current range (a file left
      // over from the other logging period) would otherwise yield a negative
      // remainder, which wraps huge when cast and gets picked as "oldest".
      const int wrap = (int)m_slotWrap;
      const uint8_t age =
          (uint8_t)((((current - fileSlot) % wrap) + wrap) % wrap);
      if (age >= oldestAge) {
        oldestAge = age;
        strncpy(oldestName, m_efs.filename(f), EFS_FILENAMELENGTH);
      }
    }
    if (oldestName[0] != 0) {
      D_print("EEPROM removing oldest file: ");
      D_println(oldestName);
      m_efs.remove(oldestName);
    }
  }

  EepromFS m_efs;
  RtcTime & m_time;
  bool m_mounted{false};
  // slot numbering wrap: 31 day-of-month slots, or 24 hour-of-day slots
  uint8_t m_slotWrap{31};

  // Which chip capacity the slot layout on this filesystem was written for.
  // The slot size is (capacity - header) / slots, recomputed from the detected
  // capacity on every mount, so a filesystem laid out for a different chip puts
  // every slot at the wrong offset. Byte 2 of the EepromFS header is free
  // (format() zeroes the header and then writes only the magic and the slot
  // count), so the capacity is recorded there and checked at mount.
  static constexpr unsigned int SIZE_MARK_ADDR = 2;
  static constexpr uint8_t SIZE_CODE_4K = 1;
  static constexpr uint8_t SIZE_CODE_32K = 2;
  static constexpr uint8_t SIZE_CODE_64K = 3;

  static uint8_t sizeCode(unsigned long bytes) {
    if (bytes >= 65536) return SIZE_CODE_64K;
    if (bytes >= 32768) return SIZE_CODE_32K;
    return SIZE_CODE_4K;
  }

  void markSize(uint8_t code) {
    m_efs.rawwrite(SIZE_MARK_ADDR, code);
    m_efs.rawflush();
  }

  // True when the mounted filesystem was laid out for the chip just detected.
  bool layoutMatches(uint8_t want) {
    const uint8_t have = m_efs.rawread(SIZE_MARK_ADDR);
    if (have == want) {
      return true;
    }
    // Filesystems written before this marker existed carry 0. Those were laid
    // out for 32 KB, which is byte-for-byte the layout a 32 KB part still
    // computes today, so adopt the marker and keep the logged data. Only a part
    // that turns out to be larger was really laid out for the wrong capacity.
    if (have == 0 && want == SIZE_CODE_32K) {
      markSize(want);
      return true;
    }
    return false;
  }

};
#endif
