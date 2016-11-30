/**
 * @file plugin.c
 * @author Snyo
 */
#include "plugin.h"

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <dlfcn.h>

#include <zlog.h>
#include <json/json.h>

#include "metadata.h"
#include "storage.h"
#include "util.h"

// return 0 for success, -1 for failure
int plugin_init(plugin_t *plugin, const char *type, char *option) {
	if(!plugin) return -1;
	memset(plugin, 0, sizeof(plugin_t));

	/* Tag */
	plugin->tag = zlog_get_category(type);
	if(!plugin->tag); 

	/* Thread variables */
	if(pthread_mutex_init(&plugin->sync, NULL) < 0
			|| pthread_mutex_init(&plugin->pike, NULL) < 0
			|| pthread_cond_init(&plugin->syncd, NULL) < 0
			|| pthread_cond_init(&plugin->poked, NULL) < 0) {
		zlog_error(plugin->tag, "Fail to initialize thread variables");
		plugin_fini(plugin);
		return -1;
	}

	/* Specify plugin */
	char dlname[20], smname[30];
	snprintf(dlname, 20, "lib%s.so", type);
	snprintf(smname, 30, "%s_plugin_init", type);
	void *dl = dlopen(dlname, RTLD_LAZY);
	int (*dynamic_plugin_init)(plugin_t *, char *) = dlsym(dl, smname);
	if(dlerror() || dynamic_plugin_init(plugin, option) < 0) {
		zlog_error(plugin->tag, "Fail to load %s", smname);
		plugin_fini(plugin);
		return -1;
	}

	return 0;
}

int plugin_fini(plugin_t *plugin) {
	if(!plugin) return -1;
	DEBUG(zlog_info(plugin->tag, "Finishing"));

	if(plugin->fini)
		plugin->fini(plugin);

	if(pthread_mutex_destroy(&plugin->sync) < 0
			|| pthread_mutex_destroy(&plugin->pike) < 0
			|| pthread_cond_destroy(&plugin->syncd) < 0
			|| pthread_cond_destroy(&plugin->poked) < 0)
		return -1;

	return 0;
}

/**
 * returns 0 for success, -1 for failure
 */
int start(plugin_t *plugin) {
	DEBUG(zlog_info(plugin->tag, "Starting"));

	plugin->alive   = 1;
	plugin->holding = 0;
	plugin->metric  = json_object_new_array();
	plugin->values  = json_object_new_object();

	plugin->next_run = 0;

	pthread_mutex_lock(&plugin->sync);
	pthread_create(&plugin->running_thread, NULL, plugin_main, plugin);

	return pthread_sync(&plugin->syncd, &plugin->sync, 1);
}

void stop(plugin_t *plugin) {
	DEBUG(zlog_info(plugin->tag, "Stopping"));
	plugin->alive = 0;

	pthread_cancel(plugin->running_thread);
	pthread_mutex_unlock(&plugin->sync);
	pthread_mutex_unlock(&plugin->pike);

	json_object_put(plugin->metric);
	json_object_put(plugin->values);
}

void restart(plugin_t *plugin) {
	DEBUG(zlog_info(plugin->tag, "Restarting"));
	stop(plugin);
	start(plugin);
}

int poke(plugin_t *plugin) {
	pthread_mutex_lock(&plugin->pike);
	pthread_cond_signal(&plugin->poked);
	pthread_mutex_unlock(&plugin->pike);

	return pthread_sync(&plugin->syncd, &plugin->sync, 1);
}

void *plugin_main(void *_plugin) {
	plugin_t *plugin = (plugin_t *)_plugin;

	pthread_mutex_lock(&plugin->sync);
	pthread_mutex_lock(&plugin->pike);
	pthread_cond_signal(&plugin->syncd);
	pthread_mutex_unlock(&plugin->sync);

	while(plugin->alive) {
		DEBUG(zlog_debug(plugin->tag, "Waiting to be poked"));
		pthread_cond_wait(&plugin->poked, &plugin->pike);
		DEBUG(zlog_debug(plugin->tag, "Poked"));

		pthread_mutex_lock(&plugin->sync);
		pthread_cond_signal(&plugin->syncd);
		pthread_mutex_unlock(&plugin->sync);

		DEBUG(zlog_debug(plugin->tag, "Start collecting"));
		DEBUG(epoch_t begin = epoch_time());
		plugin->curr_run = epoch_time();
		plugin->collect(plugin);
		plugin->next_run = plugin->curr_run + (epoch_t)(plugin->period*MSPS);
		if(plugin->holding == plugin->capacity) {
			pack(plugin);
			plugin->metric = json_object_new_array();
		}
		DEBUG(zlog_debug(plugin->tag, "Done in %llums", epoch_time()-begin));
	}

	return NULL;
}

void pack(plugin_t *plugin) {
	DEBUG(zlog_debug(plugin->tag, "Packing data"));
	json_object *payload = json_object_new_object();
	json_object_object_add(payload, "license", json_object_new_string(license));
	json_object_object_add(payload, "uuid", json_object_new_string(uuid));
	json_object_object_add(payload, "target_num", json_object_new_int(plugin->index));
	json_object_object_add(payload, "metrics", plugin->metric);
	json_object_object_add(payload, "values", plugin->values);
	storage_add(&storage, payload);

	plugin->values = json_object_new_object();
	plugin->holding = 0;
}

unsigned alive(plugin_t *plugin) {
	return plugin->alive;
}

unsigned outdated(plugin_t *plugin) {
	return plugin->next_run <= epoch_time();
}
