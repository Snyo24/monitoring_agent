/**
 * @file pluggable.h
 * @author Snyo
 */
#ifndef _PLUGGABLE_H_
#define _PLUGGABLE_H_

#include <pthread.h>

#include <json/json.h>

#include "util.h"

typedef struct _plugin plugin_t;

struct _plugin {
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
	       int num;
	const char *agent_ip;
	const char *target_ip;

	/* Timing variables */
	timestamp period;
	timestamp next_run;

	/* Metrics */
	int full_count;
	int holding;
	json_object *metric_names;
	json_object *values;

	/* Inheritance */
	void *spec;

	/* Polymorphism */
	void *tag;
	void (*job)(plugin_t *);
	void (*fini)(plugin_t *);
};

extern char license[];
extern char uuid[];
extern char os[];

plugin_t *new_plugin(const char *name);
void delete_plugin(plugin_t *plugin);

void *plugin_main(void *_plugin);

void start(plugin_t *plugin);
void stop(plugin_t *plugin);
void restart(plugin_t *plugin);
void poke(plugin_t *plugin);
void pack(plugin_t *plugin);

unsigned alive(plugin_t *plugin);
unsigned busy(plugin_t *plugin);
unsigned outdated(plugin_t *plugin);
unsigned timeup(plugin_t *plugin);

#endif