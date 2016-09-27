#ifndef _RUNNABLE_H_
#define _RUNNABLE_H_

#include "util.h"

#include <pthread.h>

typedef struct _runnable {
	/* Thread variables */
	pthread_t running_thread;

	/* Status variables */
	volatile unsigned alive : 1; 

	/* Timing variables */
	timestamp period;

	/* Inheritance */
	void *detail;

	/* Polymorphism */
	void *tag;
	void (*job)(void *);
} runnable_t;

runnable_t *new_runnable(double period);
void delete_runnable(runnable_t *app);

void *run(void *_app);

void start_runnable(runnable_t *app);

#endif