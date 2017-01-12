/**
 * @file plugin.h
 * @author Snyo
 */
#ifndef _PLUGIN_H_
#define _PLUGIN_H_

#include <pthread.h>

#include "routine.h"
#include "packet.h"
#include "util.h"

enum plugin_gather_error {ENONE, ENODATA, EPLUGUP, EPLUGDOWN};

typedef struct plugin_t {

    union {
        routine_t;
        routine_t r;
    };

    unsigned long long tid;
	const char *tip;
	const char *type;

    packet_t *working;
    packet_t *oob;

    int (*prep)(void *);
	int (*fini)(void *);

	int (*gather)(void *, packet_t *);
    int (*cmp)(void *, void *, int);
    int module_size;
    void *module;

} plugin_t;

int plugin_init(plugin_t *p);

int plugin_gather_phase(plugin_t *p);

#endif
