#if defined DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "logger.h"

static FILE* log_file = NULL;

int init_logger(const char* filename) {
    mkdir("logs", 0777);

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "logs/%s", filename);

    log_file = fopen(filepath, "a"); // Append mode
    if (log_file == NULL) {
        perror("Failed to open log file");
        return -1;
    }
    return 0;
}

void log_message(const char* level, const char* file, int line, const char* format, ...) {
    if (log_file == NULL) return;

    // Get current time
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char time_str[26];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    // Print header: [TIMESTAMP] [LEVEL] [file:line]
    fprintf(log_file, "[%s] [%s] [%s:%d] ", time_str, level, file, line);

    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    // Newline and flush immediately so logs aren't trapped in the buffer if the app crashes
    fprintf(log_file, "\n");
    fflush(log_file);
}

#endif // DEBUG_MODE