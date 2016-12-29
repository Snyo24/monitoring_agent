/**
 * @file runnable.h
 * @author Snyo
 */
#ifndef _RUNNABLE_H_
#define _RUNNABLE_H_

#include <pthread.h>

#include "util.h"

typedef struct runnable_t {
    
	volatile unsigned alive : 1; 

	void *tag;

	/* Timing */
	float tick;
    epoch_t due;
    unsigned delay : 3;

	int (*task)(void *);

    /* Thread */
	pthread_t running_thread;
	pthread_mutex_t ping_me;
	pthread_mutex_t pong_me;
	pthread_cond_t  ping;
	pthread_cond_t  pong;
    
} runnable_t;

int runnable_init(runnable_t *r);
int runnable_fini(runnable_t *r);

int runnable_start(runnable_t *r);
int runnable_stop(runnable_t *r);
int runnable_restart(runnable_t *r);

int runnable_ping(runnable_t *r);

void *runnable_main(runnable_t *r);

void runnable_change_task(runnable_t *r, void *task);

int runnable_alive(runnable_t *r);
int runnable_overdue(runnable_t *r);

#endif
