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

#include "metadata.h"
#include "storage.h"
#include "util.h"

typedef struct _plugin_set {
	unsigned up;
} plugin_set_t;

static plugin_set_t plugin_set = {0};

static int  _indexing();
static void _deindexing();

// return 0 for success, -1 for failure
int plugin_init(plugin_t *plugin, const char *type) {
	if(!plugin) return -1;
	memset(plugin, 0, sizeof(plugin_t));

	/* Tag */
	plugin->tag = zlog_get_category(type);
	if(!plugin->tag); 

	/* Thread variables */
	if(pthread_mutex_init(&plugin->sync, NULL) < 0
	|| pthread_mutex_init(&plugin->pole, NULL) < 0
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
    int (*dynamic_plugin_init)(plugin_t *) = dlsym(dl, smname);
    if(dlerror() || dynamic_plugin_init(plugin) < 0) {
		zlog_error(plugin->tag, "Fail to load %s", smname);
        plugin_fini(plugin);
        return -1;
    }

	/* Numbering */
	if((plugin->index = _indexing(&plugin_set)) < 0) {
		zlog_error(plugin->tag, "Cannot add more plugin");
		plugin_fini(plugin);
		return -1;
	}

	return 0;
}

int plugin_fini(plugin_t *plugin) {
	zlog_info(plugin->tag, "Finishing");
	if(!plugin) return -1;

	_deindexing(&plugin_set, plugin->index);

	if(plugin->fini)
		plugin->fini(plugin);

	if(pthread_mutex_destroy(&plugin->sync) < 0
	|| pthread_mutex_destroy(&plugin->pole) < 0
	|| pthread_cond_destroy(&plugin->syncd) < 0
	|| pthread_cond_destroy(&plugin->poked) < 0)
		return -1;
	
	return 0;
}

void start(plugin_t *plugin) {
	zlog_info(plugin->tag, "Starting");
	plugin->alive        = 1;
	plugin->working      = 0;

	plugin->holding      = 0;
	plugin->metric_names = json_object_new_array();
	plugin->values       = json_object_new_object();

	plugin->next_run     = get_timestamp() + plugin->period;

	pthread_mutex_lock(&plugin->sync);
	pthread_create(&plugin->running_thread, NULL, plugin_main, plugin);

	pthread_cond_wait(&plugin->syncd, &plugin->sync);
}

void stop(plugin_t *plugin) {
	zlog_info(plugin->tag, "Stopping");
	plugin->alive = 0;

	pthread_cancel(plugin->running_thread);
	pthread_mutex_unlock(&plugin->sync);
	pthread_mutex_unlock(&plugin->pole);

	json_object_put(plugin->metric_names);
	json_object_put(plugin->values);
}

void restart(plugin_t *plugin) {
	zlog_info(plugin->tag, "Restarting");
	stop(plugin);
	start(plugin);
}

void poke(plugin_t *plugin) {
	zlog_info(plugin->tag, "Poking");
	pthread_mutex_lock(&plugin->pole);
	pthread_cond_signal(&plugin->poked);
	pthread_mutex_unlock(&plugin->pole);

	// confirm the plugin starts collecting
	pthread_cond_wait(&plugin->syncd, &plugin->sync);
}

void *plugin_main(void *_plugin) {
	plugin_t *plugin = (plugin_t *)_plugin;

	pthread_mutex_lock(&plugin->sync);
	pthread_mutex_lock(&plugin->pole);
	pthread_cond_signal(&plugin->syncd);
	pthread_mutex_unlock(&plugin->sync);

	while(plugin->alive) {
		zlog_debug(plugin->tag, "Waiting to be poked");
		pthread_cond_wait(&plugin->poked, &plugin->pole);
		zlog_debug(plugin->tag, "Poked");

		pthread_mutex_lock(&plugin->sync);
		pthread_cond_signal(&plugin->syncd);
		pthread_mutex_unlock(&plugin->sync);

		plugin->working = 1;
		timestamp curr = get_timestamp();
		zlog_debug(plugin->tag, "Start collect");
		plugin->collect(plugin);
		plugin->holding++;
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
	zlog_debug(plugin->tag, "Packing data");
	json_object *payload = json_object_new_object();
	json_object_object_add(payload, "license", json_object_new_string(license));
	json_object_object_add(payload, "uuid", json_object_new_string(uuid));
	json_object_object_add(payload, "os", json_object_new_string(os));
	json_object_object_add(payload, "target_num", json_object_new_int(plugin->index));
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

int _indexing(plugin_set_t *plugin_set) {
	if(!~plugin_set->up)
		return -1;
	int c;
	for(c=0; (plugin_set->up >> c) & 0x1; c = (c+1)%(8*sizeof(unsigned)));
	plugin_set->up |= 0x1 << c ;
	return c;
}

void _deindexing(plugin_set_t *plugin_set, int c) {
	plugin_set->up &= 0 - (1<<c) - 1;
}
