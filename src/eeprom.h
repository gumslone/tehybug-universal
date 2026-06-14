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
    void format() {}
};
#else
#include <EepromFS.h>

// Slot-based data log on the external I2C EEPROM (FT24C256A, 32 KB, on the
// DS3231 RTC module). One file per day of month ("<mday>.txt"), so 32 slots
// of ~1 KB hold a full month of daily logs; when no free slot is left the
// oldest day file is removed. NOTE: begin() reuses the slot count an
// already-formatted EEPROM was created with — SLOTS only applies when
// formatting a blank chip.
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
      if (slots == 0) {
        // unformatted eeprom: create the filesystem once
        D_println("EEPROM unformatted, formatting...");
        m_efs.format(SLOTS);
        slots = m_efs.begin();
      }
      m_mounted = slots > 0;
      D_print("EEPROM filesystem mounted, slots: ");
      D_println(slots);
      readdir();
    }

  bool mounted() {
    return m_mounted;
  }

  // Erase all logged data by re-creating the filesystem (used by factory
  // reset). Re-mounts afterwards so logging can resume without a reboot.
  void format() {
    D_println("Formatting EEPROM data log...");
    m_efs.format(SLOTS);
    m_mounted = m_efs.begin() > 0;
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
    while (!m_efs.eof(f)) {
      data += (char)m_efs.fgetc(f);
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
  void removeOldestFile(uint8_t currentMday) {
    uint8_t oldestAge = 0;
    char oldestName[EFS_FILENAMELENGTH + 1] = {0};

    m_efs.dirp = 0;
    while (uint8_t f = m_efs.readdir()) {
      if (strcmp(m_efs.filename(f), INDEX_FILE) == 0) {
        continue; // never recycle the date index slot
      }
      const int fileMday = atoi(m_efs.filename(f));
      const uint8_t age = (uint8_t)((currentMday - fileMday + 31) % 31);
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

};
#endif
