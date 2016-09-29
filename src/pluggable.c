/**
 * @file pluggable.c
 * @author Snyo
 */
#include "pluggable.h"
#include "storage.h"
#include "util.h"

#include <stdlib.h>

#include <pthread.h>

#include <zlog.h>
#include <json/json.h>

/**
 * return a pointer for success, NULL for failure
 */
plugin_t *new_plugin(timestamp period) {
	if(period <= 0) return NULL;
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

	plugin->period = period;

	plugin->stored = 0;
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

	json_object_put(plugin->metric_names);
	json_object_put(plugin->values);

	free(plugin);
}

void start(plugin_t *plugin) {
	plugin->alive = 1;
	plugin->working = 0;

	plugin->next_run = 0;

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
		zlog_debug(plugin->tag, "Waiting to be poked");
		pthread_cond_wait(&plugin->poked, &plugin->access);
		zlog_debug(plugin->tag, "Poked");

		// Prevent the scheduler not to do other work before collecting start
		pthread_mutex_lock(&plugin->sync);

		if(!plugin->next_run) {
			plugin->next_run = get_timestamp() + plugin->period;
		} else {
			plugin->next_run += plugin->period;
		}

		pthread_cond_signal(&plugin->synced);
		pthread_mutex_unlock(&plugin->sync);

		plugin->working = 1;
		zlog_debug(plugin->tag, "Start job");
		plugin->job(plugin);
		zlog_debug(plugin->tag, "Finish job");
		plugin->working = 0;
	}

	return NULL;
}

void pack(plugin_t *plugin) {
	/*
	{
		"license": "license_exem4",
		"target_num": 1,
		"uuid": "550e8400-e29b-41d4-a716-446655440000",
		"agent_ip": "test",
		"target_ip": "test",
		"metrics": [
			"net_stat\/name",
			"net_stat\/byte_in",
			"net_stat\/byte_out",
			"net_stat\/packet_in"
		],
		"values": {
			"1475131143438891656": ["enp0s3", 12412125, 14020, 1059726, 5528 ],
			"1475131144438891656": [ "enp0s3", 12412125, 14020, 1059726, 5528 ]
		}
	}
	*/
	json_object *payload = json_object_new_object();
	json_object_object_add(payload, "license", json_object_new_string(license));
	json_object_object_add(payload, "uuid", json_object_new_string(uuid));
	json_object_object_add(payload, "os", json_object_new_string(os));
	json_object_object_add(payload, "target_num", json_object_new_int(plugin->num));
	json_object_object_add(payload, "agent_ip", json_object_new_string(plugin->agent_ip));
	json_object_object_add(payload, "target_ip", json_object_new_string(plugin->target_ip));
	json_object_object_add(payload, "metrics", plugin->metric_names);
	json_object_object_add(payload, "values", plugin->values);
	storage_add(&storage, payload);
}

int outdated(plugin_t *plugin) {
	return plugin->next_run <= get_timestamp();
}

int busy(plugin_t *plugin) {
	return plugin->working;
}

int timeup(plugin_t *plugin) {
	return plugin->next_run < get_timestamp();
}

int alive(plugin_t *plugin) {
	return plugin->alive;
}