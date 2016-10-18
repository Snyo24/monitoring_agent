/**
 * @file proxy.c
 * @author Snyo
 */
#include "plugins/proxy.h"

#include <stdio.h>
#include <string.h>

#include "pluggable.h"
#include "util.h"

#define PROXY_PLUGIN_TICK NS_PER_S/2

void init_proxy_plugin(proxy_plugin_t *plugin) {
	plugin->spec = malloc(1);
	if(!plugin->spec) return;
	*(int *)plugin->spec = 0;

	plugin->num = 1;
	plugin->agent_ip = "test";
	plugin->target_ip = "test";

	plugin->period = PROXY_PLUGIN_TICK;

	plugin->full_count = 5;

	// polymorphism
	plugin->collect = collect_proxy_metrics;
	plugin->fini = fini_proxy_plugin;
}

void fini_proxy_plugin(proxy_plugin_t *plugin) {
	if(plugin->spec)
		free(plugin->spec);
}

void collect_proxy_metrics(proxy_plugin_t *plugin) {
	json_object *values = json_object_new_array();

	int metric_number = 0;
}

