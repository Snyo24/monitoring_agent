#ifndef _PLUGGABLE_H_
#define _PLUGGABLE_H_

#include "util.h"

#include <pthread.h>

#include <json/json.h>

typedef struct _plugin {
	/* Thread variables */
	pthread_t       running_thread;
	pthread_mutex_t sync;
	pthread_cond_t  synced;
	pthread_mutex_t access;
	pthread_cond_t  poked;

	/* Status variables */
	volatile unsigned alive : 1;
	volatile unsigned working : 1;

	/* Target info */
	const char *name;
	const char *type;
	int        num;
	const char *agent_ip;
	const char *target_ip;

	/* Timing variables */
	timestamp period;
	timestamp next_run;
	timestamp due;

	/* Metrics */
	json_object *metric_names;
	json_object *values;

	/* Inheritance */
	void *detail;

	/* Polymorphism */
	void *zlog;
	void (*job)(void *);
} plugin_t;

plugin_t *new_plugin(double period);
void delete_plugin(plugin_t *plugin);

void *plugin_main(void *_plugin);

void start(plugin_t *plugin);
void stop(plugin_t *plugin);
void restart(plugin_t *plugin);
void poke(plugin_t *plugin);

int alive(plugin_t *plugin);
int outdated(plugin_t *plugin);
int busy(plugin_t *plugin);
int timeup(plugin_t *plugin);

#endif