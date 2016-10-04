/**
 * @file os.h
 * @author Snyo
 */
#ifndef _OS_H_
#define _OS_H_

#include "pluggable.h"

typedef plugin_t os_plugin_t;

void init_os_plugin(os_plugin_t *plugin);
void fini_os_plugin(os_plugin_t *plugin);
void collect_os_metrics(os_plugin_t *plugin);

#endif