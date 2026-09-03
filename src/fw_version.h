#pragma once
#include "common_functions.h"

// Semantic firmware version. Bump it when merging user-visible changes: the
// release workflow reads this define, tags the release v<FW_VERSION> and
// publishes it — and skips the release when the tag already exists, so a
// merge without a bump just refreshes the committed binaries.
#define FW_VERSION "1.0.0"

const String version = FW_VERSION;

// Compile timestamp (YYMMDDHHMM) — until v1.0.0 this WAS the version, now it
// only identifies the exact build (two builds of the same version, a local
// debug build vs the released one). Reported next to the version in
// /api/info.
#define COMPILE_HOUR (((__TIME__[0] - '0') * 10) + (__TIME__[1] - '0'))
#define COMPILE_MINUTE (((__TIME__[3] - '0') * 10) + (__TIME__[4] - '0'))
#define COMPILE_SHORT_YEAR (((__DATE__[9] - '0')) * 10 + (__DATE__[10] - '0'))
#define COMPILE_MONTH                                                          \
  ((__DATE__[2] == 'n'   ? (__DATE__[1] == 'a' ? 0 : 5)                        \
    : __DATE__[2] == 'b' ? 1                                                   \
    : __DATE__[2] == 'r' ? (__DATE__[0] == 'M' ? 2 : 3)                        \
    : __DATE__[2] == 'y' ? 4                                                   \
    : __DATE__[2] == 'l' ? 6                                                   \
    : __DATE__[2] == 'g' ? 7                                                   \
    : __DATE__[2] == 'p' ? 8                                                   \
    : __DATE__[2] == 't' ? 9                                                   \
    : __DATE__[2] == 'v' ? 10                                                  \
    : 11) +                                                                    \
   1)
#define COMPILE_DAY                                                            \
  ((__DATE__[4] == ' ' ? 0 : __DATE__[4] - '0') * 10 + (__DATE__[5] - '0'))

const String buildTimestamp = String(COMPILE_SHORT_YEAR) +
                              IntFormat(COMPILE_MONTH) + IntFormat(COMPILE_DAY) +
                              IntFormat(COMPILE_HOUR) + IntFormat(COMPILE_MINUTE);
