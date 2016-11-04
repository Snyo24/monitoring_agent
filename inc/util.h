/** @file util.h @author Snyo */

#ifndef _UTIL_H_
#define _UTIL_H_

#define MS_PER_S (1000000UL)

#define get_ts(_1) timestamp _1; do {\
	struct timespec now;\
	clock_gettime(CLOCK_REALTIME, &now);\
	_1 = now.tv_sec * MS_PER_S + now.tv_nsec/1000;\
} while(0)

typedef unsigned long timestamp;

timestamp get_timestamp();
void snyo_sleep(timestamp ms);
int file_exist(char *filename);

#endif
