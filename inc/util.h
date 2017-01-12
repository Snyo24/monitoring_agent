/** @file util.h @author Snyo */

#ifndef _UTIL_H_
#define _UTIL_H_

#define MSPS (1000ULL)
#define NSPMS (1000000ULL)
#define BPKB 1024

#define BFSZ 128

#ifdef VERBOSE
#define DEBUG(x) x
#else
#define DEBUG(x) do{} while(0)
#endif

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

typedef unsigned long long epoch_t;

epoch_t epoch_time();
void snyo_sleep(float sec);

#endif
