#ifndef SRC_DEBUG_LOG_H_
#define SRC_DEBUG_LOG_H_

#include <cstdio>

// Diagnostic logging gate. Set to 1 to emit the verbose per-operation trace
// output that was used while bringing up the emulator (device accesses, ADB
// traffic, Sony driver intercepts, CPU loops, etc.). Left at 0 for normal use
// so the emulator runs quietly; genuine errors are still reported via
// fprintf(stderr, ...) directly.
#ifndef EMU_DEBUG
#define EMU_DEBUG 0
#endif

#if EMU_DEBUG
#define DLOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define DLOG(...) \
  do {            \
  } while (0)
#endif

#endif  // SRC_DEBUG_LOG_H_
