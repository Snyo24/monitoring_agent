/**
 * @file scheduler.c
 * @author Snyo
 */

#include "scheduler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlog.h>
 
#include "plugin.h"
#include "util.h"

#define SCHEDULER_TICK MS_PER_S/2

typedef struct plugin_set_t {
	unsigned up;
} plugin_set_t;

static plugin_set_t plugin_set = {0};

static int  _indexing();
static void _deindexing();

int scheduler_init(scheduler_t *scheduler) {
	if(runnable_init((runnable_t *)&scheduler) < 0)
		return -1;

	scheduler->tag = zlog_get_category("scheduler");
	if(!scheduler->tag);
	
	zlog_debug(scheduler->tag, "Initailize a plugin table");
	memset(scheduler->plugins, 0, sizeof(plugin_t *)*MAX_PLUGIN);

	scheduler->period = SCHEDULER_TICK;
	scheduler->job    = scheduler_main;

	zlog_debug(scheduler->tag, "Initialize plugins");
	plugin_t *plugin = malloc(sizeof(plugin_t));
	if(plugin_init(plugin, "os") != -1) {
		plugin->index = 0;
		scheduler->plugins[plugin->index] = plugin;
	}

	FILE *plugin_conf = fopen("conf/conf", "r");
	if(!plugin_conf) {
		zlog_error(scheduler->tag, "Fail to open conf");
		return -1;
	}

	char line[1000];
	while(fgets(line, 1000, plugin_conf)) {
		char type[100], option[900];
		if(sscanf(line, "%100[^(,\n)],%900[^\n]\n", type, option) != 2)
			continue;
		plugin = malloc(sizeof(plugin_t));

		if(plugin_init(plugin, type) != -1) {
			if(strncmp(type, "proxy", 5) == 0)
				plugin->index = 1;
			else if(strncmp(type, "mysql", 5) == 0)
				plugin->index = 2;
			scheduler->plugins[plugin->index] = plugin;
		}
	}
	return 0;
}

void scheduler_fini(scheduler_t *scheduler) {
	runnable_fini((runnable_t *)scheduler);
}

void start_plugins(scheduler_t *scheduler) {
	zlog_debug(scheduler->tag, "Start plugins");
	plugin_t **plugins = scheduler->plugins;
	for(int i=0; i<MAX_PLUGIN; ++i) {
		if(plugins[i]) start(plugins[i]);
	}
}

void scheduler_main(void *_scheduler) {
	scheduler_t *scheduler = (scheduler_t *)_scheduler;

	for(int i=0; i<MAX_PLUGIN; ++i) {
		plugin_t *plugin = scheduler->plugins[i];
		if(!plugin) continue;
		zlog_debug(scheduler->tag, "Status of %d [%c%c%c%c]", \
		                                plugin->index, \
		                                alive(plugin)   ?'R':'.', \
		                                outdated(plugin)?'O':'.', \
		                                busy(plugin)    ?'B':'.', \
		                                timeup(plugin)  ?'T':'.');
		if(alive(plugin)) {
			if(timeup(plugin)) {
				zlog_error(scheduler->tag, "Timeup");
				restart(plugin);
			} else if(outdated(plugin)) {
				zlog_debug(scheduler->tag, "Poking");
				poke(plugin);
			}
		}
	}
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
