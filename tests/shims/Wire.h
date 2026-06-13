// Fake I2C bus for native host tests: emulates a 16-bit-addressed serial
// EEPROM (FT24C256A, 32 KB) backed by an in-memory array, so the real
// EepromFS driver runs unchanged on a desktop compiler.
#pragma once
#include <Arduino.h>
#include <cstdint>
#include <cstddef>
#include <deque>
#include <initializer_list>
#include <set>
#include <vector>

class FakeWire {
 public:
  static constexpr size_t SIZE = 32768; // FT24C256A = 32 KB

  void begin() {}
  void begin(int, int) {}

  // which I2C addresses acknowledge (default: just the data-log EEPROM)
  void setPresent(std::initializer_list<int> addrs) {
    m_present = std::set<int>(addrs);
  }

  void beginTransmission(int addr) {
    m_addr = addr;
    m_tx.clear();
  }

  size_t write(int b) {
    m_tx.push_back((uint8_t)(b & 0xFF));
    return 1;
  }
  size_t write(const uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++) m_tx.push_back(buf[i]);
    return len;
  }

  // First two bytes of a transmission set the address pointer; any further
  // bytes are written sequentially from there (a page/byte write).
  // Returns 0 (ack) only if the addressed device is present.
  uint8_t endTransmission() {
    const bool ack = m_present.count(m_addr) > 0;
    if (ack && m_tx.size() >= 2) {
      m_ptr = ((unsigned)m_tx[0] << 8) | m_tx[1];
      for (size_t i = 2; i < m_tx.size(); i++) {
        if (m_ptr < SIZE) m_mem[m_ptr] = m_tx[i];
        m_ptr++;
      }
    }
    m_tx.clear();
    return ack ? 0 : 2; // 2 = NACK on address
  }

  int requestFrom(int addr, int n) {
    m_rx.clear();
    if (m_present.count(addr) == 0) return 0;
    for (int i = 0; i < n; i++) {
      m_rx.push_back(m_ptr < SIZE ? m_mem[m_ptr] : 0xFF);
      m_ptr++;
    }
    return n;
  }
  int available() { return (int)m_rx.size(); }
  int read() {
    if (m_rx.empty()) return -1;
    int v = m_rx.front();
    m_rx.pop_front();
    return v;
  }

  // test helper: wipe the simulated chip (fresh, unformatted EEPROM) and
  // reset the present devices to just the EEPROM
  void wipe() {
    for (size_t i = 0; i < SIZE; i++) m_mem[i] = 0;
    m_present = {0x50};
  }

 private:
  uint8_t m_mem[SIZE] = {0};
  unsigned m_ptr = 0;
  int m_addr = 0;
  std::set<int> m_present{0x50};
  std::vector<uint8_t> m_tx;
  std::deque<uint8_t> m_rx;
};

inline FakeWire Wire;
