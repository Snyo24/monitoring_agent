/**
 * @file scheduler.c
 * @author Snyo
 */

#include "scheduler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlog.h>
 
#include "pluggable.h"
#include "util.h"

#define SCHEDULER_TICK NS_PER_S/2
#define MAX_PLUGIN 10

int scheduler_init(scheduler_t *scheduler) {
	if(runnable_init(scheduler, SCHEDULER_TICK) < 0)
		return -1;

	scheduler->tag = zlog_get_category("scheduler");
	if(!scheduler->tag);
	
	if(!(scheduler->spec = malloc(sizeof(plugin_t *)*MAX_PLUGIN)))
		return -1;
	memset(scheduler->spec, 0, sizeof(plugin_t *)*MAX_PLUGIN);

	scheduler->job = scheduler_main;

	plugin_t *plugin = malloc(sizeof(plugin_t));
	if(plugin_init(plugin, "proxy") != -1) {
		start(plugin);
		((plugin_t **)scheduler->spec)[plugin->index] = plugin;
	}
	plugin = malloc(sizeof(plugin_t));
	//if(plugin_init(plugin, "proxy") != -1) {
	//	start(plugin);
	//	((plugin_t **)scheduler->spec)[plugin->index] = plugin;
//	}

	return 0;
}

void scheduler_fini(scheduler_t *scheduler) {
	free(scheduler->spec);
}

void scheduler_main(void *_scheduler) {
	scheduler_t *scheduler = (scheduler_t *)_scheduler;
	plugin_t **plugins = (plugin_t **)scheduler->spec;

	for(int i=0; i<MAX_PLUGIN; ++i) {
		plugin_t *plugin = plugins[i];
		if(!plugin) continue;
		zlog_debug(scheduler->tag, "Status of %d [%c%c%c%c]", \
		                                plugin->index, \
		                                alive(plugin)?'R':'.', \
		                                outdated(plugin)?'O':'.', \
		                                busy(plugin)?'B':'.', \
		                                busy(plugin)&&timeup(plugin)?'T':'.');
		if(alive(plugin)) {
			if(busy(plugin)) {
				if(timeup(plugin)) {
					zlog_fatal(scheduler->tag, "Timeup");
					//restart(plugin);
				}
			} else if(outdated(plugin)) {
				zlog_debug(scheduler->tag, "Time to collect");
				poke(plugin);
			}
		}
	}
}
