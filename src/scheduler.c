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

#define SCHEDULER_TICK 0.5

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

	DEBUG(zlog_debug(scheduler->tag, "Initailize a plugin table"));
	memset(scheduler->plugins, 0, sizeof(plugin_t *)*MAX_PLUGIN);

	scheduler->period = SCHEDULER_TICK;
	scheduler->job    = scheduler_main;

	FILE *plugin_conf = fopen("cfg/plugins", "r");
	if(!plugin_conf) {
		zlog_error(scheduler->tag, "Fail to open conf");
		return -1;
	}

	DEBUG(zlog_debug(scheduler->tag, "Initialize plugins"));
	char line[1000];
	plugin_t *plugin;
	while(fgets(line, 1000, plugin_conf)) {
		char type[100], option[900];
		if(sscanf(line, "%100[^(,\n)],%900[^\n]\n", type, option) != 2)
			continue;
		plugin = malloc(sizeof(plugin_t));

		if(plugin_init(plugin, type) != -1) {
			if(strncmp(type, "os", 2) == 0)
				plugin->index = 0;
			else if(strncmp(type, "proxy", 5) == 0)
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
	DEBUG(zlog_debug(scheduler->tag, "Start plugins"));
	plugin_t **plugins = scheduler->plugins;
	for(int i=0; i<MAX_PLUGIN; ++i) {
		if(plugins[i]) 
			if(start(plugins[i]) < 0) {
				plugin_fini(plugins[i]);
				plugins[i] = NULL;
			}
	}
}

void scheduler_main(void *_scheduler) {
	scheduler_t *scheduler = (scheduler_t *)_scheduler;

	for(int i=0; i<MAX_PLUGIN; ++i) {
		plugin_t *plugin = scheduler->plugins[i];
		if(!plugin) continue;
		DEBUG(zlog_debug(scheduler->tag, "Plugin_%d [%c%c] (%d/%d)", \
				plugin->index, \
				alive(plugin)   ?'A':'.', \
				outdated(plugin)?'O':'.', \
				plugin->holding,\
				plugin->capacity));

		if(alive(plugin) && outdated(plugin)) {
			DEBUG(zlog_debug(scheduler->tag, "Poking"));
			if(poke(plugin) < 0) {
				zlog_error(scheduler->tag, "Cannot poke the agent");
				restart(plugin);
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
