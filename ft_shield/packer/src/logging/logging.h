#ifndef __LOGGING__H__
#define __LOGGING__H__

#define DEBUG_LOGLV 1
#define INFO_LOGLV 2
#define WARN_LOGLV 3
#define ERR_LOGLV 4


#define LOGLEVEL INFO_LOGLV


void info(const char* format, ...);
void debug(const char* format, ...);
void error(const char* format, ...);
void warn(const char* format, ...);

#endif  //!__LOGGING__H__
