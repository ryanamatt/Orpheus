#ifndef LOGGER_H
#define LOGGER_H

#ifdef DEBUG

int init_logger(const char* filename);

void log_message(const char* level, const char* file, int line, const char* format, ...);

// Public macros that capture the file name and line number automatically
#define log_info(fmt, ...)  log_message("INFO",  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define log_debug(fmt, ...) log_message("DEBUG", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define log_error(fmt, ...) log_message("ERROR", __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#else

// If not in debug mode, these macros do absolutely nothing
#define init_logger(filename) ((void)0)
#define log_info(fmt, ...) ((void)0)
#define log_debug(fmt, ...) ((void)0)
#define log_error(fmt, ...) ((void)0)

#endif // DEBUG

#endif // LOGGER_H