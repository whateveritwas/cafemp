#include "logger/logger.hpp"
#include <stdarg.h>

#define COLOR_GREEN  "\x1b[32m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_RED    "\x1b[31m"
#define COLOR_BLUE   "\x1b[34m"
#define COLOR_RESET  "\x1b[0m"

static const char* get_level_color(LogLevel level) {
    switch (level) {
        case LOG_OK:      return COLOR_GREEN;
        case LOG_WARNING: return COLOR_YELLOW;
        case LOG_ERROR:   return COLOR_RED;
        case LOG_DEBUG:   return COLOR_BLUE;
        default:          return COLOR_RESET;
    }
}

static const char* get_level_label(LogLevel level) {
    switch (level) {
        case LOG_OK:      return "OK";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR:   return "ERROR";
        case LOG_DEBUG:   return "DEBUG";
        default:          return "UNKNOWN";
    }
}

void log_message(LogLevel level, const char* system, const char* format, ...) {
    const char* color = get_level_color(level);
    const char* label = get_level_label(level);

    printf("%s[%s] [%s] ", color, system, label);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("%s\n", COLOR_RESET);
}
