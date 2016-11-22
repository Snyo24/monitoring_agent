/**
 * @file mysql.c
 * @author Snyo
 */
#include "plugins/mysql.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <json/json.h>
#include <mysql/mysql.h>

#include "plugin.h"
#include "util.h"

#define MYSQL_PLUGIN_TICK 5
#define MYSQL_PLUGIN_CAPACITY 1

typedef struct _mysql_spec {
	MYSQL *mysql;
} mysql_spec_t;

static void collect_mysql(plugin_t *plugin);

static void _collect_crud(plugin_t *plugin, json_object *values);
static void _collect_query(plugin_t *plugin, json_object *values);
static void _collect_innodb(plugin_t *plugin, json_object *values);
static void _collect_threads(plugin_t *plugin, json_object *values);
static void _collect_replica(plugin_t *plugin, json_object *values);
static MYSQL_RES *query_result(MYSQL *mysql, const char *query);

int mysql_plugin_init(plugin_t *plugin, char *option) {
    char host[50], root[50], pass[50];
    if(sscanf(option, "%50[^,],%50[^,],%50[^,]\n", host, root, pass) != 3)
        return -1;
	mysql_spec_t *spec = malloc(sizeof(mysql_spec_t));
	if(!spec) return -1;

	if(!(spec->mysql = mysql_init(NULL))) {
		free(spec);
		return -1;
	}

	if(!mysql_real_connect(spec->mysql, host, root, pass, NULL, 0, NULL, 0)) {
		mysql_close(spec->mysql);
		free(spec);
		return -1;
	}

	plugin->spec = spec;

	plugin->type     = "mysql_linux_1.0";
	plugin->ip       = 0;
	plugin->period   = MYSQL_PLUGIN_TICK;
	plugin->capacity = MYSQL_PLUGIN_CAPACITY;

	plugin->collect  = collect_mysql;
	plugin->fini     = mysql_plugin_fini;

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

void collect_mysql(plugin_t *plugin) {
	json_object *values = json_object_new_array();

	_collect_crud(plugin, values);
	_collect_query(plugin, values);
	_collect_innodb(plugin, values);
	_collect_threads(plugin, values);
	_collect_replica(plugin, values);

	char ts[20];
	snprintf(ts, 20, "%llu", epoch_time());
	json_object_object_add(plugin->values, ts, values);
	plugin->holding ++;
}

void _collect_crud(plugin_t *plugin, json_object *values) {
	MYSQL_RES *res = query_result(((mysql_spec_t *)plugin->spec)->mysql, \
			"show global status where variable_name in ('com_select','com_insert','com_update','com_delete');");

	MYSQL_ROW row;
	while((row = mysql_fetch_row(res))) {
		json_object_array_add(plugin->metric, json_object_new_string(row[0]));
		json_object_array_add(values, json_object_new_int(atoi(row[1])));
	}
	mysql_free_result(res);
    
    res = query_result(((mysql_spec_t *)plugin->spec)->mysql, \
            "select sum(variable_value) value from information_schema.global_status where variable_name like 'com_alter%' union all select sum(variable_value) value from information_schema.global_status where variable_name like 'com_create%' union all select sum(variable_value) value from information_schema.global_status where variable_name like 'com_drop%';");
    json_object_array_add(plugin->metric, json_object_new_string("Com_alter"));
    json_object_array_add(plugin->metric, json_object_new_string("Com_create"));
    json_object_array_add(plugin->metric, json_object_new_string("Com_drop"));
    while((row = mysql_fetch_row(res))) {
        json_object_array_add(values, json_object_new_int(atoi(row[0])));
    }
	mysql_free_result(res);
}

void _collect_query(plugin_t *plugin, json_object *values) {
	MYSQL_RES *res = query_result(((mysql_spec_t *)plugin->spec)->mysql, \
			"show global status where variable_name in (\
					 'slow_queries'\
			);");
	MYSQL_ROW row;
	while((row = mysql_fetch_row(res))) {
		json_object_array_add(plugin->metric, json_object_new_string(row[0]));
		json_object_array_add(values, json_object_new_int(atoi(row[1])));
	}
	mysql_free_result(res);
    /*
	res = query_result(((mysql_spec_t *)plugin->spec)->mysql, \
			"show global variables where variable_name in (\
		  'slow_query_log',\
			'slow_query_log_file'\
			);");
	int slow_query_log = 0;
	char slow_query_log_file[100];
	while((row = mysql_fetch_row(res))) {
		if(strncmp("slow_query_log_file", row[0], 19) == 0) {
			strncpy(slow_query_log_file, row[1], 100);
		} else if(strncmp("slow_query_log", row[0], 14) == 0) {
			slow_query_log = strncmp("ON", row[1], 2)==0;
		}
	}
	if(slow_query_log) {
		FILE *log = fopen(slow_query_log_file, "r");
		if(log) fclose(log);
	}
    */
    res = query_result(((mysql_spec_t *)plugin->spec)->mysql, \
            "SELECT concat_ws(',', user_host, ifnull(sql_text, ''),TIME_TO_SEC(query_time),DATE_FORMAT(start_time, '%Y-%m-%d %H:%i:%s'),rows_sent,rows_examined) FROM mysql.slow_log WHERE sql_text NOT LIKE '%#MOC#%' LIMIT 100;");
    int num = 0;
    while ((row = mysql_fetch_row(res))) {
        char slow_q[100];
        sprintf(slow_q, "Slow_query|%d", num++);
		json_object_array_add(plugin->metric, json_object_new_string(slow_q));
		json_object_array_add(values, json_object_new_string(row[0]));
    }
}

void _collect_innodb(plugin_t *plugin, json_object *values) {
	MYSQL_RES *res = query_result(((mysql_spec_t *)plugin->spec)->mysql, \
			"show global variables where variable_name in (\
					 'innodb_buffer_pool_size'\
			);");
	MYSQL_ROW row;
	while((row = mysql_fetch_row(res))) {
		json_object_array_add(plugin->metric, json_object_new_string(row[0]));
		json_object_array_add(values, json_object_new_int(atoi(row[1])));
	}
	mysql_free_result(res);
	res = query_result(((mysql_spec_t *)plugin->spec)->mysql, \
			"show global status where variable_name in (\
			'innodb_buffer_pool_pages_dirty',\
			'innodb_buffer_pool_pages_free',\
			'innodb_buffer_pool_pages_data',\
			'innodb_buffer_pool_read_requests',\
			'innodb_buffer_pool_reads',\
			'innodb_buffer_pool_write_requests',\
			'innodb_pages_created',\
			'innodb_pages_read',\
			'innodb_pages_written'\
			);");

	while((row = mysql_fetch_row(res))) {
		json_object_array_add(plugin->metric, json_object_new_string(row[0]));
		json_object_array_add(values, json_object_new_int(atoi(row[1])));
	}
	mysql_free_result(res);
    res = query_result(((mysql_spec_t *)plugin->spec)->mysql, \
            "show engine innodb status;");
    row = mysql_fetch_row(res);
    char *match = "Ibuf";
    int total, free, used;
    for(int i=0; i<strlen(row[2]); ++i) {
        if(strncmp(row[2]+i, match, 4) == 0)
            if(sscanf(row[2]+i, "Ibuf: size %d, free list len %d, seg size %d", &used, &free, &total) == 3) {
                json_object_array_add(plugin->metric, json_object_new_string("Cell_Count"));
                json_object_array_add(plugin->metric, json_object_new_string("Free_Cells"));
                json_object_array_add(plugin->metric, json_object_new_string("Used_Cells"));
                json_object_array_add(values, json_object_new_int(total));
                json_object_array_add(values, json_object_new_int(free));
                json_object_array_add(values, json_object_new_int(used));
                printf("%d %d %d\n\n", total, free, used);
            }
    }
    mysql_free_result(res);
}

void _collect_threads(plugin_t *plugin, json_object *values) {
	MYSQL_RES *res = query_result(((mysql_spec_t *)plugin->spec)->mysql, \
			"show global status where variable_name in (\
					 'connections',\
			'threads_connected',\
			'threads_running'\
			);");
	MYSQL_ROW row;
	while((row = mysql_fetch_row(res))) {
		json_object_array_add(plugin->metric, json_object_new_string(row[0]));
		json_object_array_add(values, json_object_new_int(atoi(row[1])));
	}
	mysql_free_result(res);

    res = query_result(((mysql_spec_t *)plugin->spec)->mysql, \
	   "SELECT a.id, concat_ws(',', ifnull(b.thread_id, ''), ifnull(a.info, ''), ifnull(a.user, ''), ifnull(a.host, ''), ifnull(a.db, ''), a.time, ifnull(round(c.timer_wait/1000000000000,3), ''), ifnull(c.event_id, ''), ifnull(c.event_name, ''), a.command, a.state) FROM information_schema.processlist a left join performance_schema.threads b on a.id=b.processlist_id left join performance_schema.events_waits_current c on b.thread_id=c.thread_id WHERE  1=1 and ( a.info is null or  a.info not like '%#MOC#%')");
	   while((row = mysql_fetch_row(res))) {
           char thread[100];
           sprintf(thread, "Thread|%s\n", row[0]);
           json_object_array_add(plugin->metric, json_object_new_string(thread));
           json_object_array_add(values, json_object_new_string(row[1]));
	   }
	   mysql_free_result(res);
}

void _collect_replica(plugin_t *plugin, json_object *values) {
}

MYSQL_RES *query_result(MYSQL *mysql, const char *query) {
	if(mysql_query(mysql, query)) return NULL;
	return mysql_store_result(mysql);
}
