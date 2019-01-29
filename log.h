#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TRACE 0
#define LOG_DEBUG 1
#define LOG_INFO  2
#define LOG_WARN  3
#define LOG_ERROR 4

#define LEVEL_NAME(level)


#define _LOG_(level, format, ...) \
 do{										\
	 if(level < log_level) break;				\
	 char* levelNameArray[5] = {"TRACE","DEBUG", "INFO","WARN","ERROR"}; \
	 size_t len = strlen(format);					\
	 const char* prefix = "[%s]%s(%d)-<%s>: "; \
	 size_t prefixLen = strlen(prefix);	\
	 char* fmt = malloc(len + prefixLen +1);		\
	 memset(fmt, 0, len + prefixLen +1);			\
	 strcpy(fmt, prefix);				\
	 strcpy(fmt+prefixLen, format);				\
	 printf(fmt,levelNameArray[level],__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__ );	\
	 free(fmt);	\
 }while(0)

