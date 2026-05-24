#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "logging.h"

void info(const char* format, ...) {
    if (LOGLEVEL > INFO_LOGLV)
        return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    printf("\033[32m[INFO %04d-%02d-%02d %02d:%02d:%02d]\033[0m ",
       t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
       t->tm_hour, t->tm_min, t->tm_sec);
    
    va_list args;
    va_start(args, format);

    vprintf(format, args);

    va_end(args);

    printf("\n");
}

void warn(const char* format, ...) {
    if (LOGLEVEL > WARN_LOGLV)
        return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    printf("\033[33m[WARN %04d-%02d-%02d %02d:%02d:%02d]\033[0m ",
       t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
       t->tm_hour, t->tm_min, t->tm_sec);
    
    va_list args;
    va_start(args, format);

    vprintf(format, args);

    va_end(args);

    printf("\n");
}


void error(const char* format, ...) {
    if (LOGLEVEL > ERR_LOGLV)
        return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    printf("\033[31m[ERROR %04d-%02d-%02d %02d:%02d:%02d]\033[0m ",
       t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
       t->tm_hour, t->tm_min, t->tm_sec);
    
    va_list args;
    va_start(args, format);

    vprintf(format, args);

    va_end(args);

    printf("\n");
}


void debug(const char* format, ...) {
    if (LOGLEVEL > DEBUG_LOGLV)
        return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    printf("\033[90m[DEBUG %04d-%02d-%02d %02d:%02d:%02d]\033[0m ",
       t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
       t->tm_hour, t->tm_min, t->tm_sec);
    
    va_list args;
    va_start(args, format);

    vprintf(format, args);

    va_end(args);

    printf("\n");
}