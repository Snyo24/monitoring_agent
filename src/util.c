/** @file util.c @author Snyo */

#include "util.h"

#include <stdio.h>

// for timestamp, sleep
#include <time.h>

// for file existance
#include <sys/stat.h>

timestamp get_timestamp() {
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	return now.tv_sec * MS_PER_S + now.tv_nsec/1000;
}

void snyo_sleep(timestamp ms) {
	struct timespec timeout = {
		ms/MS_PER_S,
		ms%MS_PER_S*1000
	};
	nanosleep(&timeout, NULL);
}

int file_exist(char *filename) {
	struct stat st;
	return !stat(filename, &st);
}
