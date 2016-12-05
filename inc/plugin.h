/**
 * @file plugin.h
 * @author Snyo
 */
#ifndef _PLUGIN_H_
#define _PLUGIN_H_

#include <pthread.h>

#include <json/json.h>

#include "util.h"

typedef struct plugin_t plugin_t;

struct plugin_t {
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

    int tgc;
    void *tgv[5];

    char *packet;

	/* Timing variables */
	float period;
	union {
        epoch_t next_run;
        epoch_t curr_run;
    };

	/* Inheritance */
	void *spec;

	/* Polymorphism */
	void *tag;
	int (*collect)(void *, char *);
	void (*fini)(void *);
};

int plugin_init(plugin_t *plugin);
int plugin_fini(plugin_t *plugin);

void *plugin_main(void *_plugin);

int  start(plugin_t *plugin);
void stop(plugin_t *plugin);
void restart(plugin_t *plugin);
int  poke(plugin_t *plugin);
void pack(char *packet);

unsigned alive(plugin_t *plugin);
unsigned outdated(plugin_t *plugin);

#endif
