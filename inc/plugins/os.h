/**
 * @file os.h
 * @author Snyo
 */
#ifndef _OS_H_
#define _OS_H_

#include "pluggable.h"

typedef plugin_t os_plugin_t;

int  os_plugin_init(plugin_t *plugin);
void os_plugin_fini(plugin_t *plugin);

void collect_os_metrics(plugin_t *plugin);

#endif
