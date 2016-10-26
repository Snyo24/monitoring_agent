/**
 * @file scheduler.h
 * @author Snyo 
 * @brief Schedule plugins
 */

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include "runnable.h"

typedef runnable_t scheduler_t;

int  scheduler_init(scheduler_t *scheduler);
void scheduler_fini(scheduler_t *scheduler);
void start_plugins(scheduler_t *scheduler);

void scheduler_main(void *_scheduler);

#endif
