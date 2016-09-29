/**
 * @file scheduler.c
 * @author Snyo
 */

#include "scheduler.h"
#include "pluggable.h"
#include "shash.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlog.h>

#define SCHEDULER_TICK NS_PER_S/4

int scheduler_init(scheduler_t *scheduler) {
	if(runnable_init(scheduler, SCHEDULER_TICK) < 0) return -1;

	scheduler->tag = zlog_get_category("Scheduler");
	if(!scheduler->tag);

	
	if(!(scheduler->spec = malloc(sizeof(shash_t)))
		|| shash_init(scheduler->spec) < 0)
		return -1;

	scheduler->job = scheduler_main;

	return 0;
}

void scheduler_fini(scheduler_t *scheduler) {
	shash_fini(scheduler->spec);
	free(scheduler->spec);
}

void scheduler_main(void *_scheduler) {
	scheduler_t *scheduler = (scheduler_t *)_scheduler;
	shash_t *plugins = scheduler->spec;

	for(int i=0; i<1; ++i) {
		plugin_t *plugin = (plugin_t *)shash_search(plugins, "os");
		if(!plugin) continue;
		zlog_debug(scheduler->tag, "Status of \'%s\' [%c%c%c%c]", \
		                                plugin->agent_ip, \
		                                alive(plugin)?'R':'.', \
		                                outdated(plugin)?'O':'.', \
		                                busy(plugin)?'B':'.', \
		                                busy(plugin)&&timeup(plugin)?'T':'.');
		if(alive(plugin)) {
			if(busy(plugin)) {
				if(timeup(plugin)) {
					zlog_fatal(scheduler->tag, "Restarting \'%s\'", plugin->agent_ip);
					restart(plugin);
				}
			} else if(outdated(plugin)) {
				zlog_debug(scheduler->tag, "Poking \'%s\'", plugin->agent_ip);
				poke(plugin);
			}
		}
	}
}
