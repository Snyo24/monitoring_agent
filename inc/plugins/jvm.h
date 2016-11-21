/**
 * @file jvm.h
 * @author Snyo
 */
#ifndef _JVM_H_
#define _JVM_H_

#include "plugin.h"

typedef plugin_t jvm_plugin_t;

int  jvm_plugin_init(plugin_t *plugin, char *option);
void jvm_plugin_fini(plugin_t *plugin);

void collect_jvm_metrics(plugin_t *plugin);

#endif
