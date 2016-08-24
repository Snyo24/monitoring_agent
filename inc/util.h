/** @file util.h @author Snyo */

#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdint.h>

#define GIGA ((uint64_t)1000000000)

typedef uint64_t timestamp;

timestamp get_timestamp();
void snyo_sleep(timestamp ns);
int file_exist(char *filename);
void yaml_parser(const char *file, char *result);

#endif