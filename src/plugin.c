/**
 * @file plugin.c
 * @author Snyo
 */
#include "plugin.h"

#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include <zlog.h>
#include <json/json.h>

#include "metadata.h"
#include "storage.h"
#include "util.h"

// return 0 for success, -1 for failure
int plugin_init(plugin_t *plugin) {
	if(!plugin) return -1;

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
	plugin->next_run = 0;

	pthread_mutex_lock(&plugin->sync);
	pthread_create(&plugin->running_thread, NULL, plugin_main, plugin);

	if(pthread_sync(&plugin->syncd, &plugin->sync, 1) < 0)
        return -1;

    pthread_mutex_lock(&plugin->pike);
    pthread_mutex_unlock(&plugin->pike);
    return 0;
}

void stop(plugin_t *plugin) {
	DEBUG(zlog_info(plugin->tag, "Stopping"));
	plugin->alive = 0;

	pthread_cancel(plugin->running_thread);
	pthread_mutex_unlock(&plugin->sync);
	pthread_mutex_unlock(&plugin->pike);
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
        for(int i=0; i<plugin->tgc; i++) {
            int n = 0;
            plugin->packet = malloc(4096);
            DEBUG(epoch_t begin = epoch_time());
            plugin->curr_run = epoch_time();
            n += sprintf(plugin->packet+n, "{\"ts\":%llu,\"target\":%d,\"values\":{", plugin->curr_run, i);
            n += plugin->collect(plugin->tgv[i], plugin->packet+n);
            n += sprintf(plugin->packet+n, "}}");
            plugin->next_run = plugin->curr_run + (epoch_t)(plugin->period*MSPS);
            pack(plugin->packet);
            DEBUG(zlog_debug(plugin->tag, "Done in %llums", epoch_time()-begin));
        }
	}

	return NULL;
}

void pack(char *packet) {
	storage_add(&storage, packet);
}

unsigned alive(plugin_t *plugin) {
	return plugin->alive;
}

unsigned outdated(plugin_t *plugin) {
	return plugin->next_run <= epoch_time();
}
