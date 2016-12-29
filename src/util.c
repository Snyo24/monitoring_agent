/** @file util.c @author Snyo */

#include "util.h"

#include <time.h>
#include <sys/time.h>

/**
 * Millisecond resolution
 */
epoch_t epoch_time() {
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	return now.tv_sec * MSPS + now.tv_nsec/NSPMS;
}

void snyo_sleep(float sec) {
    time_t tv_sec = (time_t)sec;
    long tv_nsec = (sec-tv_sec)*NSPMS*MSPS; 
	struct timespec timeout = {tv_sec, tv_nsec};
	nanosleep(&timeout, NULL);
}

