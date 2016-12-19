/**
 * @file scheduler.h
 * @author Snyo 
 * @brief Schedule plugins
 */

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include "runnable.h"
#include "plugin.h"

typedef struct scheduler_t {
	runnable_t;

    int pluginc;
	void **plugins;
} scheduler_t;

int  scheduler_init(scheduler_t *scheduler, int pluginc, void **plugins);
void scheduler_fini(scheduler_t *scheduler);

int scheduler_main(void *_scheduler);

#endif
