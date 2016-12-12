/**
 * @file scheduler.h
 * @author Snyo 
 * @brief Schedule targets
 */

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include "runnable.h"
#include "target.h"

#define MAX_PLUGIN 10

typedef struct scheduler_t {
	runnable_t;
    int targetc;
	void *targets[MAX_PLUGIN];
} scheduler_t;

int  scheduler_init(scheduler_t *scheduler);
void scheduler_fini(scheduler_t *scheduler);

void scheduler_main(void *_scheduler);

#endif
