/**
 * @file routine.h
 * @author Snyo
 */
#ifndef _ROUTINE_H_
#define _ROUTINE_H_

#include <pthread.h>

#include "util.h"

typedef struct routine_t {
    
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
    
} routine_t;

int routine_init(routine_t *r);
int routine_fini(routine_t *r);

int routine_start(routine_t *r);
int routine_stop(routine_t *r);
int routine_restart(routine_t *r);

int routine_ping(routine_t *r);

void *routine_main(routine_t *r);

void routine_change_task(routine_t *r, void *task);

int routine_alive(routine_t *r);
int routine_overdue(routine_t *r);

#endif
