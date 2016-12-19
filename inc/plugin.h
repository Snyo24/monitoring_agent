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
    runnable_t;

    unsigned long long tid;
	const char *tip;
	const char *type;

    packet_t *packets;
    packet_t *writing;

    int (*exec)(void *);
	int (*fini)(void *);
	int (*gather)(void *, char *);
} plugin_t;

int plugin_init(void *_p);

#endif
