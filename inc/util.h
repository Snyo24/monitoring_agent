/** @file util.h @author Snyo */

#ifndef _UTIL_H_
#define _UTIL_H_

#include <pthread.h>

#define MSPS (1000ULL)
#define NSPMS (1000000ULL)

#ifdef VERBOSE
#define DEBUG(x) x
#else
#define DEBUG(x) do{} while(0)
#endif

typedef unsigned long long epoch_t;

epoch_t epoch_time();
void snyo_sleep(float sec);
int pthread_sync(pthread_cond_t *syncd,\
		         pthread_mutex_t *sync,\
				 unsigned long timelimit);
int file_exist(char *filename);

#endif
