#pragma once

// Serial debug output. Off by default; the build script can override it
// with a compiler flag (-DDEBUG=1), see build.sh debug mode.
#ifndef DEBUG
#define DEBUG 0
#endif

#if DEBUG
#define D_SerialBegin(...) Serial.begin(__VA_ARGS__)
#define D_print(...) Serial.print(__VA_ARGS__)
#define D_write(...) Serial.write(__VA_ARGS__)
#define D_println(...) Serial.println(__VA_ARGS__)
#else
#define D_SerialBegin(...)
#define D_print(...)
#define D_write(...)
#define D_println(...)
#endif
