#pragma once
#include <EepromFS.h>
#include "debug.h"
#include "rtc_time.h"

// Slot-based data log on an external I2C EEPROM (e.g. AT24C32 on a
// DS3231 RTC module). One file per day of month ("<mday>.txt"); when no
// free slot is left the oldest day file is removed.
class TeHyBugEeprom{
  public :
  static constexpr uint8_t SLOTS = 8;

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
  // filesystem is full; returns false when nothing could be written
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
    bool written = false;
    for (size_t i = 0; i < line.length(); i++) {
      if (!m_efs.fputc(line[i], f)) {
        break; // slot full: keep what fits, the rest of the day is dropped
      }
      written = true;
    }
    m_efs.fflush(f);
    m_efs.fclose(f);
    return written;
  }

  // JSON array of the stored files: [{"name":"10.txt","size":123},...]
  String listFilesJson() {
    String json = "[";
    if (m_mounted) {
      m_efs.dirp = 0;
      bool first = true;
      while (uint8_t f = m_efs.readdir()) {
        if (!first) {
          json += ",";
        }
        first = false;
        json += "{\"name\":\"" + String(m_efs.filename(f)) + "\",\"size\":" +
                String(m_efs.filesize(f)) + "}";
      }
    }
    json += "]";
    return json;
  }

  private:

  // removes the day file furthest in the past relative to currentMday
  void removeOldestFile(uint8_t currentMday) {
    uint8_t oldestAge = 0;
    char oldestName[EFS_FILENAMELENGTH + 1] = {0};

    m_efs.dirp = 0;
    while (uint8_t f = m_efs.readdir()) {
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
