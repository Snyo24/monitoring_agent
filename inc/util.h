/** @file util.h @author Snyo */

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdint.h>
#include <zlog.h>

#define NANO ((uint64_t)1000000000)

typedef uint64_t timestamp;
typedef enum _log_level {
	DEBUG,
	INFO,
	NOTICE,
	WARN,
	ERROR,
	FATAL,
	UNKOWN
} log_level_t;

timestamp get_timestamp();
void yaml_parser(const char *file, char *result);

#endif