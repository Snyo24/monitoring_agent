/**
 * @file mysql.h
 * @author Snyo
 */
#ifndef _MYSQL_H_
#define _MYSQL_H_

#include "plugin.h"

typedef plugin_t mysql_plugin_t;

int  mysql_plugin_init(plugin_t *plugin);
void mysql_plugin_fini(plugin_t *plugin);

void collect_mysql_metrics(plugin_t *plugin);

#endif
