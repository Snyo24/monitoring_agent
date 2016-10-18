/**
 * @file pluggable.c
 * @author Snyo
 */
#include "pluggable.h"

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <dlfcn.h>

#include <zlog.h>
#include <json/json.h>

#include "storage.h"
#include "util.h"

/**
 * return a pointer for success, NULL for failure
 */
plugin_t *new_plugin(const char *name) {
	/* Allocation */
	plugin_t *plugin = malloc(sizeof(plugin_t));
	if(!plugin) return NULL;
	memset(plugin, 0, sizeof(plugin_t));

	/* Tag */
	plugin->tag = zlog_get_category(name);
	if(!plugin->tag);

	/* Thread variables */
	if(pthread_mutex_init(&plugin->sync, NULL)
	  || pthread_cond_init(&plugin->synced, NULL)
	  || pthread_mutex_init(&plugin->access, NULL)
	  || pthread_cond_init(&plugin->poked, NULL)) {
		delete_plugin(plugin);
		return NULL;
	}

	/* Specify plugin */
	char tmp[100];
	snprintf(tmp, 100, "lib%s.so", name);
    void *handle = dlopen(tmp, RTLD_LAZY);
	snprintf(tmp, 100, "init_%s_plugin", name);
    void (*init_plugin)(plugin_t *) = dlsym(handle, tmp);
    if (dlerror()) {
		zlog_error(plugin->tag, "Init plugin failed %s", name);
        delete_plugin(plugin);
        return NULL;
    }
    init_plugin(plugin);

	return plugin;
}

void delete_plugin(plugin_t *plugin) {
	if(!plugin) return;

	if(plugin->fini)
		plugin->fini(plugin);

	// The mutex, cond will be unset regardless of its state.
	pthread_mutex_destroy(&plugin->sync);
	pthread_cond_destroy(&plugin->synced);
	pthread_mutex_destroy(&plugin->access);
	pthread_cond_destroy(&plugin->poked);

	json_object_put(plugin->metric_names);
	json_object_put(plugin->values);

	free(plugin);
}

void start(plugin_t *plugin) {
	plugin->alive = 1;
	plugin->working = 0;

	plugin->holding = 0;
	plugin->metric_names = json_object_new_array();
	plugin->values = json_object_new_object();

	plugin->next_run = get_timestamp();

	pthread_mutex_lock(&plugin->sync);
	pthread_create(&plugin->running_thread, NULL, plugin_main, plugin);

	pthread_cond_wait(&plugin->synced, &plugin->sync);
}

void stop(plugin_t *plugin) {
	plugin->alive = 0;

	json_object_put(plugin->metric_names);
	json_object_put(plugin->values);
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

void *plugin_main(void *_plugin) {
	plugin_t *plugin = (plugin_t *)_plugin;

	pthread_mutex_lock(&plugin->sync);
	pthread_mutex_lock(&plugin->access);
	pthread_cond_signal(&plugin->synced);
	pthread_mutex_unlock(&plugin->sync);

	while(plugin->alive) {
		zlog_debug(plugin->tag, "Waiting to be poked");
		pthread_cond_wait(&plugin->poked, &plugin->access);
		zlog_debug(plugin->tag, "Poked");

		pthread_mutex_lock(&plugin->sync);
		pthread_cond_signal(&plugin->synced);
		pthread_mutex_unlock(&plugin->sync);

		plugin->working = 1;
		timestamp curr = get_timestamp();
		zlog_debug(plugin->tag, "Start collect");
		plugin->collect(plugin);
		if(plugin->holding == plugin->full_count) {
			pack(plugin);
			plugin->metric_names = json_object_new_array();
		}
		zlog_debug(plugin->tag, "Finish collect (%lums)", (get_timestamp()-curr)/1000000);
		plugin->next_run += plugin->period;
		plugin->working = 0;
	}

	return NULL;
}

void pack(plugin_t *plugin) {
	json_object *payload = json_object_new_object();
	json_object_object_add(payload, "license", json_object_new_string(license));
	json_object_object_add(payload, "uuid", json_object_new_string(uuid));
	json_object_object_add(payload, "os", json_object_new_string(os));
	json_object_object_add(payload, "target_num", json_object_new_int(plugin->num));
	json_object_object_add(payload, "metrics", plugin->metric_names);
	json_object_object_add(payload, "values", plugin->values);
	storage_add(&storage, payload);

	plugin->values = json_object_new_object();
	plugin->holding = 0;
}

unsigned alive(plugin_t *plugin) {
	return plugin->alive;
}

unsigned busy(plugin_t *plugin) {
	return plugin->working;
}

unsigned outdated(plugin_t *plugin) {
	return plugin->next_run <= get_timestamp();
}

unsigned timeup(plugin_t *plugin) {
	return plugin->next_run < get_timestamp();
}
