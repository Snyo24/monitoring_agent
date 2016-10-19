/**
 * @file proxy.c
 * @author Snyo
 */
#include "plugins/proxy.h"

#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>

#include "pluggable.h"
#include "util.h"

#define PROXY_PLUGIN_TICK NS_PER_S/2
#define MAX_CONNECTION 3

void proxy_plugin_init(proxy_plugin_t *plugin) {
	plugin->spec = malloc(sizeof(int));
	if(!plugin->spec) return;

	*(int *)plugin->spec = epoll_create(MAX_CONNECTION * 2);

	plugin->num = 1;
	plugin->target_ip = "test";

	plugin->period = PROXY_PLUGIN_TICK;

	plugin->full_count = 5;

	plugin->collect = collect_proxy_metrics;
	plugin->fini = proxy_plugin_fini;
}

void proxy_plugin_fini(proxy_plugin_t *plugin) {
	if(!plugin)
		return;

	if(plugin->spec)
		free(plugin->spec);
}

void collect_proxy_metrics(proxy_plugin_t *plugin) {
	json_object *values = json_object_new_array();

	int metric_number = 0;
} 
