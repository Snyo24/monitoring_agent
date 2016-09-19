/** @file util.h @author Snyo */

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdint.h>

#define NS_PER_S (1000000000UL)
#define NUMBER_OF(arr) (sizeof(arr)/sizeof(arr[0]))

typedef unsigned long timestamp;

timestamp get_timestamp();
void snyo_sleep(timestamp ns);
int file_exist(char *filename);
void yaml_parser(const char *file, char *result);

#endif