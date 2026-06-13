// Fake I2C bus for native host tests: emulates a 16-bit-addressed serial
// EEPROM (FT24C256A, 32 KB) backed by an in-memory array, so the real
// EepromFS driver runs unchanged on a desktop compiler.
#pragma once
#include <Arduino.h>
#include <cstdint>
#include <cstddef>
#include <deque>
#include <vector>

class FakeWire {
 public:
  static constexpr size_t SIZE = 32768; // FT24C256A = 32 KB

  void begin() {}
  void begin(int, int) {}

  void beginTransmission(int) { m_tx.clear(); }

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
  uint8_t endTransmission() {
    if (m_tx.size() >= 2) {
      m_ptr = ((unsigned)m_tx[0] << 8) | m_tx[1];
      for (size_t i = 2; i < m_tx.size(); i++) {
        if (m_ptr < SIZE) m_mem[m_ptr] = m_tx[i];
        m_ptr++;
      }
    }
    m_tx.clear();
    return 0; // ack
  }

  int requestFrom(int, int n) {
    m_rx.clear();
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

  // test helper: wipe the simulated chip (fresh, unformatted EEPROM)
  void wipe() {
    for (size_t i = 0; i < SIZE; i++) m_mem[i] = 0;
  }

 private:
  uint8_t m_mem[SIZE] = {0};
  unsigned m_ptr = 0;
  std::vector<uint8_t> m_tx;
  std::deque<uint8_t> m_rx;
};

inline FakeWire Wire;
