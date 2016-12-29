/**
 * @file plugin.h
 * @author Snyo
 */
#ifndef _PLUGIN_H_
#define _PLUGIN_H_

#include <pthread.h>

#include "runnable.h"
#include "packet.h"
#include "util.h"

typedef struct plugin_t {

    union {
        runnable_t;
        runnable_t r;
    };

    unsigned long long tid;
	const char *tip;
	const char *type;

    packet_t *packets;
    packet_t *working;
    packet_t *oob;

    int (*prep)(void *);
	int (*fini)(void *);

	int (*gather)(void *, packet_t *);
    void *module;

} plugin_t;

int plugin_init(plugin_t *p);

int plugin_gather_phase(plugin_t *p);

#endif
