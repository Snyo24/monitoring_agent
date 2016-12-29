/**
 * @file sch.c
 * @author Snyo
 */

#include "scheduler.h"
#include "runnable.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlog.h>

#include "plugin.h"
#include "util.h"

#define SCHEDULER_TICK 0.53

int scheduler_main(void *_sch);

int scheduler_init(runnable_t *sch) {
	if(runnable_init(sch) < 0)
		return -1;

	sch->tag = zlog_get_category("scheduler");
	if(!sch->tag);

	sch->tick = SCHEDULER_TICK;
    sch->task = scheduler_main;
	
	return 0;
}

void scheduler_fini(runnable_t *sch) {
	runnable_fini(sch);
}

int scheduler_main(void *_sch) {
    runnable_t *sch = _sch;

    extern int pluginc;
    extern plugin_t *plugins[];
    
	for(int i=0; i<pluginc; i++) {
		void *p = plugins[i];
		if(!p) break;

		if(runnable_alive(p) && runnable_overdue(p)) {
            DEBUG(zlog_debug(sch->tag, "Plugin_%d", i));
			if(runnable_ping(p) < 0 && plugin_gather_phase(p)) {
				zlog_error(sch->tag, "Cannot ping the plugin");
				runnable_restart(p);
			}
		}
	}
    return 0;
}
