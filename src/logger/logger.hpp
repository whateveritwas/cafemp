#ifndef LOGGER_HPP
#define LOGGER_HPP

typedef enum { LOG_OK, LOG_WARNING, LOG_ERROR, LOG_DEBUG } LogLevel;

void log_message(LogLevel level, const char *system, const char *format, ...);

#endif
