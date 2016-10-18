/**
 * @file proxy.h
 * @author Snyo
 */
#ifndef _PROXY_H_
#define _PROXY_H_

#include "pluggable.h"

typedef plugin_t proxy_plugin_t;

void init_proxy_plugin(proxy_plugin_t *plugin);
void fini_proxy_plugin(proxy_plugin_t *plugin);
void collect_proxy_metrics(proxy_plugin_t *plugin);

#endif
