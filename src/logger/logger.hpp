#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

typedef enum {
    LOG_OK,
    LOG_WARNING,
    LOG_ERROR,
    LOG_DEBUG
} LogLevel;

void log_message(LogLevel level, const char* system, const char* format, ...);

#endif /* SRC_LOGGER_LOGGER_HPP_ */
