/** @file network_agent.h @author Snyo */

#ifndef _NETWORK_H_
#define _NETWORK_H_

#include "pluggable.h"

typedef struct plugin_t network_plugin_t;

typedef struct _network_detail {
} network_detail_t;

plugin_t *new_network_plugin(const char *name, const char *conf);
void collect_network_metrics(plugin_t *network_agent);
void delete_network_agent(plugin_t *network_agent);

#endif