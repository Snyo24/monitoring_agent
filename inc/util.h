/** @file util.h @author Snyo */

#ifndef _UTIL_H_
#define _UTIL_H_

#define MS_PER_S (1000ULL)

typedef unsigned long long timestamp;

timestamp get_timestamp();
void snyo_sleep(timestamp ms);
int file_exist(char *filename);

#endif
