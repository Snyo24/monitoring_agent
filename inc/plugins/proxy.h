/**
 * @file proxy.h
 * @author Snyo
 */
#ifndef _PROXY_H_
#define _PROXY_H_

#include "plugin.h"

typedef plugin_t proxy_plugin_t;

int  proxy_plugin_init(proxy_plugin_t *plugin);
void proxy_plugin_fini(proxy_plugin_t *plugin);

void collect_proxy_metrics(proxy_plugin_t *plugin);

#endif
