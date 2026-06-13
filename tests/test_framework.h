// Minimal header-only assertion framework for the native tests. Each test
// executable has its own main() and its own pass/fail counters.
#pragma once
#include <cstdio>
#include <string>

inline int g_pass = 0, g_fail = 0;
inline const char *g_case = "";

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
#define CHECK_EQ_STR(a, b)                                            \
  do {                                                               \
    std::string _a((a)), _b((b));                                    \
    if (_a == _b) {                                                  \
      g_pass++;                                                      \
    } else {                                                         \
      g_fail++;                                                      \
      std::printf("  FAIL [%s] %s:%d: \"%s\" != \"%s\"\n", g_case,   \
                  __FILE__, __LINE__, _a.c_str(), _b.c_str());      \
    }                                                                \
  } while (0)
#define SUMMARY() \
  (std::printf("\n%d passed, %d failed\n", g_pass, g_fail), g_fail == 0 ? 0 : 1)
