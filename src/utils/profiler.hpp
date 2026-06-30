#ifndef PROFILER
#define PROFILER

#include <coreinit/time.h>
#include "logger/logger.hpp"

typedef struct {
    const char *name;
    uint64_t start;
} profiler;

static inline void profiler_begin(profiler *p, const char *name) {
    p->name = name;
    p->start = OSGetSystemTime();
}

static inline void profiler_end(profiler *p) {
    uint64_t end = OSGetSystemTime();

    uint64_t ticks = end - p->start;
    uint64_t us = OSTicksToMicroseconds(ticks);

    log_message(LOG_PROFILER, "Profiler", "%-24s %8llu us (%llu ticks)", p->name, us, ticks);
}

#endif
