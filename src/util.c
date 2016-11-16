/** @file util.c @author Snyo */

#include "util.h"

#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>

/**
 * Millisecond resolution
 */
epoch_t epoch_time() {
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	return now.tv_sec * MSPS + now.tv_nsec/NSPMS;
}

void snyo_sleep(epoch_t ms) {
	struct timespec timeout = {
		ms/MSPS,
		ms%MSPS*NSPMS
	};
	nanosleep(&timeout, NULL);
}

int pthread_sync(pthread_cond_t *syncd,\
                 pthread_mutex_t *sync,\
				 unsigned long timelimit) {
	struct timespec timeout;
	clock_gettime(CLOCK_REALTIME, &timeout);
	timeout.tv_sec += timelimit;
	return (pthread_cond_timedwait(syncd, sync, &timeout)==0) - 1;
}

int file_exist(char *filename) {
	struct stat st;
	return !stat(filename, &st);
}
