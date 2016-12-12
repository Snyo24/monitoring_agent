/*o
 * @file runnable.h
 * @author Snyo
 */
#ifndef _RUNNABLE_H_
#define _RUNNABLE_H_

#include <pthread.h>

#include "util.h"

typedef struct runnable_t {
	pthread_t running_thread;

	volatile unsigned alive : 1; 

	float period;

	void *tag;

	void (*job)(void *);
} runnable_t;

int  runnable_init(runnable_t *app);
void runnable_fini(runnable_t *app);

void *run(void *_app);

void start_runnable(runnable_t *app);

#endif
