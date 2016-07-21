#include "util.h"

#include <time.h>

timestamp get_timestamp() {
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	return now.tv_sec * NANO + now.tv_nsec;
}