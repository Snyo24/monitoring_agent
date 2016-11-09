/**
 * @file plugin.h
 * @author Snyo
 */
#ifndef _PLUGIN_H_
#define _PLUGIN_H_

#include <pthread.h>

#include <json/json.h>

#include "util.h"

typedef struct _plugin plugin_t;

struct _plugin {
	int index;
	
	/* Thread variables */
	pthread_t       running_thread;
	pthread_mutex_t sync;
	pthread_mutex_t fork;
	pthread_cond_t  syncd;
	pthread_cond_t  poked;

	/* Status variables */
	volatile unsigned alive : 1;
	volatile unsigned working : 1;

	/* Target info */
	char *target_type;
	char *target_ip;

	/* Timing variables */
	unsigned long period;
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
	void (*collect)(plugin_t *);
	void (*fini)(plugin_t *);
};

int plugin_init(plugin_t *plugin, const char *type);
int plugin_fini(plugin_t *plugin);

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

void alert(plugin_t *plugin);

#endif
