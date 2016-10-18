/**
 * @file runnable.h
 * @author Snyo
 */
#ifndef _RUNNABLE_H_
#define _RUNNABLE_H_

#include <pthread.h>

#include "util.h"

typedef struct _runnable {
	/* Thread variables */
	pthread_t running_thread;

	/* Status variables */
	volatile unsigned alive : 1; 

	/* Timing variables */
	timestamp period;

	/* Inheritance */
	void *spec;

	/* Polymorphism */
	void *tag;
	void (*collect)(void *);
} runnable_t;

int  runnable_init(runnable_t *app, timestamp period);
void runnable_fini(runnable_t *app);

void *run(void *_app);

void start_runnable(runnable_t *app);

#endif