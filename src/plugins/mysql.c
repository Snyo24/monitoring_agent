/**
 * @file mysql.c
 * @author Snyo
 */
#include "plugins/mysql.h"

#include <stdio.h>
#include <string.h>

#include <mysql/mysql.h>

#include "pluggable.h"
#include "util.h"

#define MYSQL_PLUGIN_TICK MS_PER_S*3
#define MYSQL_PLUGIN_FULL 1

typedef struct _mysql_spec {
	MYSQL *mysql;
} mysql_spec_t;

static MYSQL_RES *query_result(MYSQL *mysql, const char *query);

int mysql_plugin_init(plugin_t *plugin) {
	mysql_spec_t *spec = malloc(sizeof(mysql_spec_t));
	if(!spec) return -1;
	
	if(!(spec->mysql = mysql_init(NULL))) {
		free(spec);
		return -1;
	}

	if(!mysql_real_connect(spec->mysql, "localhost", "root", "snyo", NULL, 0, NULL, 0)) {
		mysql_close(spec->mysql);
		free(spec);
		return -1;
	}

	plugin->spec = spec;

	plugin->target_type = "mysql_linux_1.0";
	plugin->target_ip  = 0;
	plugin->period     = MYSQL_PLUGIN_TICK;
	plugin->full_count = MYSQL_PLUGIN_FULL;

	plugin->collect    = collect_mysql_metrics;
	plugin->fini       = mysql_plugin_fini;

	return 0;
}

void mysql_plugin_fini(plugin_t *plugin) {
	if(!plugin)
		return;
	
	if(plugin->spec) {
		mysql_close(((mysql_spec_t *)plugin->spec)->mysql);
		free(plugin->spec);
	}
}

void collect_mysql_metrics(plugin_t *plugin) {
	json_object *values = json_object_new_array();
	MYSQL_RES *res = query_result(((mysql_spec_t *)plugin->spec)->mysql, "show global status where variable_name in ('Queries');");

	MYSQL_ROW row;
	while((row = mysql_fetch_row(res)))
		printf("%s %s\n", row[0], row[1]);

	mysql_free_result(res);

	char ts[20];
	snprintf(ts, 20, "%lu", plugin->next_run - plugin->period);
	json_object_object_add(plugin->values, ts, values);
	plugin->holding ++;
}

MYSQL_RES *query_result(MYSQL *mysql, const char *query) {
	if(mysql_query(mysql, query)) return NULL;
	return mysql_store_result(mysql);
}
