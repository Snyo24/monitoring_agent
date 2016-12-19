/**
 * @file sched.c
 * @author Snyo
 */

#include "scheduler.h"
#include "runnable.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include <zlog.h>

#include "plugin.h"
#include "util.h"

#define SCHEDULER_TICK 0.53

typedef struct plugin_set_t {
	unsigned up;
} plugin_set_t;

//static plugin_set_t plugin_set = {0};

//static int  _indexing();
//static void _deindexing();

int scheduler_init(scheduler_t *sched, int pluginc, void **plugins) {
	if(runnable_init((runnable_t *)sched) < 0)
		return -1;

	sched->tag = zlog_get_category("scheduler");
	if(!sched->tag);

	sched->tick    = SCHEDULER_TICK;
	sched->routine = scheduler_main;
    sched->pluginc = pluginc;
    sched->plugins = plugins;
	
	return 0;
}

void scheduler_fini(scheduler_t *sched) {
	runnable_fini((runnable_t *)sched);
}

int scheduler_main(void *_sched) {
	scheduler_t *sched = (scheduler_t *)_sched;

	for(int i=0; i<sched->pluginc; i++) {
		void *p = sched->plugins[i];
		if(!p) break;
		DEBUG(zlog_debug(sched->tag, "Plugin_%d [%c%c]", \
				i, \
				runnable_alive(p)  ?'A':'.', \
				runnable_overdue(p)?'O':'.'));

		if(runnable_alive(p) && runnable_overdue(p)) {
			DEBUG(zlog_debug(sched->tag, "Ping"));
			if(runnable_ping(p) < 0) {
				zlog_error(sched->tag, "Cannot ping the plugin");
				runnable_restart(p);
			}
		}
	}
    return 0;
}

int _indexing(plugin_set_t *plugin_set) {
	if(!~plugin_set->up)
		return -1;
	int c;
	for(c=0; (plugin_set->up >> c) & 0x1; c = (c+1)%(8*sizeof(plugin_set_t)));
	plugin_set->up |= 0x1 << c ;
	return c;
}

void _deindexing(plugin_set_t *plugin_set, int c) {
	plugin_set->up &= 0 - (1<<c) - 1;
}
