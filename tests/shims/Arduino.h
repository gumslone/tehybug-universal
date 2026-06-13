// Minimal Arduino.h shim for native host tests. Provides just enough of the
// Arduino core (the String class, a couple of free functions) to compile and
// run the project's hardware-independent logic on a desktop compiler.
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

inline void delay(unsigned long) {}
inline void delayMicroseconds(unsigned long) {}
inline unsigned long millis() { return 0; }
inline void yield() {}

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

// Arduino String, backed by std::string, implementing the subset the project
// uses. Methods mirror the Arduino API (indexOf/substring/startsWith/...).
class String {
 public:
  String() {}
  String(const char *s) : m_s(s ? s : "") {}
  String(const std::string &s) : m_s(s) {}
  String(char c) : m_s(1, c) {}
  String(int v) : m_s(std::to_string(v)) {}
  String(unsigned int v) : m_s(std::to_string(v)) {}
  String(long v) : m_s(std::to_string(v)) {}
  String(unsigned long v) : m_s(std::to_string(v)) {}
  String(unsigned char v) : m_s(std::to_string((int)v)) {}
  // String(value, base) — Arduino formats non-decimal bases as lowercase
  String(long v, int base) {
    if (base == 10) {
      m_s = std::to_string(v);
      return;
    }
    unsigned long u = (unsigned long)v;
    if (u == 0) {
      m_s = "0";
      return;
    }
    const char *digits = "0123456789abcdef";
    std::string out;
    while (u > 0) {
      out = std::string(1, digits[u % (unsigned)base]) + out;
      u /= (unsigned)base;
    }
    m_s = out;
  }

  unsigned int length() const { return (unsigned int)m_s.size(); }
  const char *c_str() const { return m_s.c_str(); }
  void reserve(unsigned int n) { m_s.reserve(n); }
  char operator[](int i) const { return m_s[(size_t)i]; }

  int indexOf(char c, int from = 0) const {
    size_t p = m_s.find(c, (size_t)from);
    return p == std::string::npos ? -1 : (int)p;
  }
  int indexOf(const String &s, int from = 0) const {
    size_t p = m_s.find(s.m_s, (size_t)from);
    return p == std::string::npos ? -1 : (int)p;
  }
  String substring(int from) const { return String(m_s.substr((size_t)from)); }
  String substring(int from, int to) const {
    if (to < from) return String();
    return String(m_s.substr((size_t)from, (size_t)(to - from)));
  }
  bool startsWith(const String &s) const { return m_s.rfind(s.m_s, 0) == 0; }
  bool startsWith(const char *s) const { return m_s.rfind(s, 0) == 0; }

  String &operator+=(const String &s) { m_s += s.m_s; return *this; }
  String &operator+=(const char *s) { m_s += s; return *this; }
  String &operator+=(char c) { m_s += c; return *this; }

  bool operator==(const String &o) const { return m_s == o.m_s; }
  bool operator==(const char *s) const { return m_s == s; }
  bool operator!=(const String &o) const { return m_s != o.m_s; }

  friend String operator+(const String &a, const String &b) {
    return String(a.m_s + b.m_s);
  }
  friend String operator+(const String &a, const char *b) {
    return String(a.m_s + b);
  }
  friend String operator+(const char *a, const String &b) {
    return String(std::string(a) + b.m_s);
  }
  friend String operator+(const String &a, char b) {
    return String(a.m_s + b);
  }

  const std::string &std_str() const { return m_s; }

 private:
  std::string m_s;
};
