/**
 * @file plugin.c
 * @author Snyo
 */
#include "pluggable.h"
#include "util.h"

#include <stdlib.h>

#include <pthread.h>

#include <zlog.h>
#include <json/json.h>

/**
 * return a pointer for success, NULL for failure
 */
plugin_t *new_plugin(double period) {
	// allocation
	plugin_t *plugin = (plugin_t *)malloc(sizeof(plugin_t));
	if(!plugin) return NULL;

	if(pthread_mutex_init(&plugin->sync, NULL) \
	  || pthread_cond_init(&plugin->synced, NULL) \
	  || pthread_mutex_init(&plugin->access, NULL) \
	  || pthread_cond_init(&plugin->poked, NULL)) {
		delete_plugin(plugin);
		return NULL;
	}

	plugin->alive = 0;

	plugin->period = period * NS_PER_S;
	plugin->next_run = 0;

	plugin->metric_names = json_object_new_array();
	plugin->values = json_object_new_object();

	return plugin;
}

void delete_plugin(plugin_t *plugin) {
	if(!plugin) return;

	// The mutex, cond will be unset regardless of its state.
	pthread_mutex_destroy(&plugin->sync);
	pthread_cond_destroy(&plugin->synced);
	pthread_mutex_destroy(&plugin->access);
	pthread_cond_destroy(&plugin->poked);

	free(plugin);
}

void start(plugin_t *plugin) {
	plugin->alive = 1;
	plugin->working = 0;

	plugin->next_run = 0;
	plugin->due = 0;

	pthread_mutex_lock(&plugin->sync);
	pthread_create(&plugin->running_thread, NULL, plugin_main, plugin);

	pthread_cond_wait(&plugin->synced, &plugin->sync);
}

void stop(plugin_t *plugin) {
	plugin->alive = 0;

	pthread_cancel(plugin->running_thread);
	pthread_mutex_unlock(&plugin->sync);
	pthread_mutex_unlock(&plugin->access);
}

void restart(plugin_t *plugin) {
	stop(plugin);
	start(plugin);
}

void poke(plugin_t *plugin) {
	pthread_mutex_lock(&plugin->access);
	pthread_cond_signal(&plugin->poked);
	pthread_mutex_unlock(&plugin->access);

	// confirm the plugin starts collecting
	pthread_cond_wait(&plugin->synced, &plugin->sync);
}

void *plugin_main(void *_app) {
	plugin_t *plugin = (plugin_t *)_app;

	pthread_mutex_lock(&plugin->sync);
	pthread_mutex_lock(&plugin->access);
	pthread_cond_signal(&plugin->synced);
	pthread_mutex_unlock(&plugin->sync);

	while(plugin->alive) {
		pthread_cond_wait(&plugin->poked, &plugin->access);

		// Prevent the scheduler not to do other work before collecting start
		pthread_mutex_lock(&plugin->sync);

		plugin->due = get_timestamp() + plugin->period;

		pthread_cond_signal(&plugin->synced);
		pthread_mutex_unlock(&plugin->sync);

		plugin->working = 1;
		plugin->job(plugin);
		plugin->working = 0;

		if(!plugin->next_run) {
			plugin->next_run = get_timestamp();
		} else {
			plugin->next_run += plugin->period;
		}
	}

	return NULL;
}

int outdated(plugin_t *plugin) {
	return plugin->next_run <= get_timestamp();
}

int busy(plugin_t *plugin) {
	return plugin->working;
}

int timeup(plugin_t *plugin) {
	return plugin->due < get_timestamp();
}

int alive(plugin_t *plugin) {
	return plugin->alive;
}