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
	pthread_mutex_t pike;
	pthread_cond_t  syncd;
	pthread_cond_t  poked;

	/* Status variables */
	volatile unsigned alive : 1;

	/* Target info */
	char *type;
	char *ip;

	/* Timing variables */
	float period;
	union {
        epoch_t next_run;
        epoch_t curr_run;
    };

	/* Metrics */
	int capacity;
	int holding;
	json_object *metric;
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

int  start(plugin_t *plugin);
void stop(plugin_t *plugin);
void restart(plugin_t *plugin);
int  poke(plugin_t *plugin);
void pack(plugin_t *plugin);

unsigned alive(plugin_t *plugin);
unsigned outdated(plugin_t *plugin);

#endif
