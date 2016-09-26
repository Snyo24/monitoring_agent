
#include "util.h"

typedef struct _runnable {
	/* Thread variables */
	pthread_t       running_thread;
	pthread_mutex_t sync;   // Synchronization
	pthread_cond_t  synced;
	pthread_mutex_t access; // Read, Write
	pthread_cond_t  poked;
	
	/* Logging */
	void *tag;

	/* Status variables */
	volatile unsigned alive : 1;
	volatile unsigned working : 1;

	/* Time */
	unsigned int period;
	timestamp    next_run;
	timestamp    due;

	/* Inheritance */
	void *detail;
} runnable_t;