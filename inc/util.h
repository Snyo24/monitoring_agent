#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdint.h>

#define NANO ((uint64_t)1000000000)

typedef uint64_t timestamp;

timestamp get_timestamp();

#endif