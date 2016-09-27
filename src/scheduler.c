/** @file scheduler.c @author Snyo */

#include "scheduler.h"
#include "plugins/network.h"
#include "shash.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#include <pthread.h>

#include <zlog.h>

int scheduler_init(scheduler_t *scheduler) {
	scheduler->tag = zlog_get_category("Scheduler");
	if(!scheduler->tag);

	scheduler->detail = (scheduling_detail_t *)malloc(sizeof(scheduling_detail_t));
	if(!scheduler->detail) return -1;

	scheduler->alive = 0;
	scheduler->period = NS_PER_S / 3;
	scheduler->job = scheduler_main;

	return 0;
}

void scheduler_main(void *_scheduler) {
	scheduler_t *scheduler = (scheduler_t *)_scheduler;

	for(int i=0; i<1; ++i) {
		zlog_debug(scheduler->tag, "%lu", scheduler->period);
		// plugin_t *plugin = (plugin_t *)shash_search(plugins, "");
		// if(!plugin) continue;
		// zlog_debug(scheduler->tag, "Status of \'%s\' [%c%c%c%c]", \
		//                                 "", \
		//                                 alive(plugin)?'R':'.', \
		//                                 outdated(plugin)?'O':'.', \
		//                                 busy(plugin)?'B':'.', \
		//                                 busy(plugin)&&timeup(plugin)?'T':'.');
		// if(alive(plugin)) {
		// 	if(busy(plugin)) {
		// 		if(timeup(plugin)) {
		// 			zlog_fatal(scheduler->tag, "Restarting \'%s\'", plugin->name);
		// 			restart(plugin);
		// 		}
		// 	} else if(outdated(plugin)) {
		// 		zlog_debug(scheduler->tag, "Poking \'%s\'", plugin->name);
		// 		poke(plugin);
		// 	}
		// }
	}
}

void scheduler_fini(scheduler_t *scheduler) {
	// TODO
}
