/**
 * @file scheduler.h
 * @author Snyo 
 * @brief Schedule agents
 */

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include "runnable.h"
#include "shash.h"

typedef runnable_t scheduler_t;
typedef struct _scheduling_detail {
	shash_t *plugins;
} scheduling_detail_t;

int  scheduler_init(scheduler_t *scheduler);
void scheduler_main(void *_scheduler);
void scheduler_fini(scheduler_t *scheduler);

#endif