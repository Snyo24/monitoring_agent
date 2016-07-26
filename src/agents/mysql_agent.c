#include "mysql_agent.h"

#include "agent.h"
#include "metric.h"
#include "snyohash.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#include <mysql.h>
#include <zlog.h>

char *mysql_metadata[] = {
	"version",
	"hostname",
	"bind_address",
	"port"
};

char *mysql_metrics[] = {
	"Com_delete",
	"Com_delete_multi",
	"Com_insert",
	"Com_insert_select",
	"Com_replace_select",
	"Com_select",
	"Com_update",
	"Com_update_multi",
	"Com_stmt_close",
	"Com_stmt_execute",
	"Com_stmt_fetch",
	"Com_stmt_prepare",
	"Com_execute_sql",
	"Com_prepare_sql",
	"Queries"
};

// TODO exception
agent_t *new_mysql_agent(int period, const char *conf) {
	// logging
	zlog_category_t *_log_tag = zlog_get_category("agent_mysql");
    if (!_log_tag) return NULL;

    zlog_debug(_log_tag, "Create a new mysql agent with period %d", period);

	agent_t *mysql_agent = new_agent(period);
	if(!mysql_agent) {
		zlog_error(_log_tag, "Failed to create a general agent");
		return NULL;
	}

	// mysql detail setup
	mysql_detail_t *detail = (mysql_detail_t *)malloc(sizeof(mysql_detail_t));
	if(!detail) {
		zlog_error(_log_tag, "Failed to allocate mysql detail");
		mysql_agent->delete_agent(mysql_agent);
		return NULL;
	}

	detail->mysql = mysql_init(NULL);
	if(!detail->mysql) {
		zlog_error(_log_tag, "Failed to mysql_init");
		free(detail);
		mysql_agent->delete_agent(mysql_agent);
		return NULL;
	}

	// TODO user setup
	char result[4][50];
	yaml_parser(conf, (char *)result);
	if(!(mysql_real_connect(detail->mysql, result[0], result[1], result[2], result[3], 0, NULL, 0))) {
		zlog_error(_log_tag, "Failed to mysql_real_connect");
		mysql_close(detail->mysql);
		free(detail);
		delete_agent(mysql_agent);
		return NULL;
	}

	// logging
	mysql_agent->log_tag = (void *)_log_tag;

	// inheritance
	mysql_agent->detail = detail;

	// polymorphism
	mysql_agent->metric_number = sizeof(mysql_metrics)/sizeof(char *);
	mysql_agent->metrics = mysql_metrics;
	mysql_agent->metadata_number = sizeof(mysql_metadata)/sizeof(char *);
	mysql_agent->metadata_list = mysql_metadata;

	mysql_agent->thread_main = mysql_main;

	mysql_agent->collect_metadata = collect_mysql_metadata;
	mysql_agent->collect_metrics = collect_mysql_metrics;

	mysql_agent->delete_agent = delete_mysql_agent;

	return mysql_agent;
}

void *mysql_main(void *_agent) {
	agent_t *agent = (agent_t *)_agent;
	zlog_debug(agent->log_tag, "MySQL thread is started");
	pthread_mutex_lock(&agent->sync);
	zlog_info(agent->log_tag, "Aquire access");
	pthread_mutex_lock(&agent->access);
	zlog_debug(agent->log_tag, "MySQL agent collects metadata");
	// Collect metadata
	agent->collect_metadata(agent);
	pthread_cond_signal(&agent->synced);
	pthread_mutex_unlock(&agent->sync);

	run(agent);
	return NULL;
}

void collect_mysql_metadata(agent_t *mysql_agent) {
	MYSQL_RES *res = query_result(((mysql_detail_t *)mysql_agent->detail)->mysql, \
		"show global variables where variable_name in (\
                                                    \'hostname\', \
                                                    \'bind_address\', \
                                                    \'port\', \
                                                    \'version\');");

	if(!res) {
		zlog_error(mysql_agent->log_tag, "Fail to get query result");
		exit(0);
	}

	hash_insert(mysql_agent->meta_buf, "hostname", NULL);
	hash_insert(mysql_agent->meta_buf, "bind_address", NULL);
	hash_insert(mysql_agent->meta_buf, "port", NULL);
	hash_insert(mysql_agent->meta_buf, "version", NULL);

	MYSQL_ROW row;
	while((row = mysql_fetch_row(res)))
		hash_insert(mysql_agent->meta_buf, row[0], new_metric(STRING, 0, row[1]));

	mysql_free_result(res);
}

void collect_mysql_metrics(agent_t *mysql_agent) {
	MYSQL_RES *res = query_result(((mysql_detail_t *)mysql_agent->detail)->mysql, \
		"show global status where variable_name in (\
                                                    \'Com_delete\',\
                                                    \'Com_delete_multi\',\
                                                    \'Com_insert\',\
                                                    \'Com_insert_select\',\
                                                    \'Com_replace_select\',\
                                                    \'Com_select\',\
                                                    \'Com_update\',\
                                                    \'Com_update_multi\',\
                                                    \'Com_stmt_close\',\
                                                    \'Com_stmt_execute\',\
                                                    \'Com_stmt_fetch\',\
                                                    \'Com_stmt_prepare\',\
                                                    \'Com_execute_sql\',\
                                                    \'Com_prepare_sql\',\
                                                    \'Queries\');");

	if(!res) {
		zlog_error(mysql_agent->log_tag, "Fail to get query result");
		exit(0);
	}

	hash_t *hash_table = next_storage(mysql_agent);
	for(size_t i=0; i<mysql_agent->metric_number; ++i)
		hash_insert(hash_table, mysql_agent->metrics[i], NULL);

	MYSQL_ROW row;
	while((row = mysql_fetch_row(res))) {
		int x = atoi(row[1]);
		hash_insert(hash_table, row[0], new_metric(INTEGER, 0, &x));
	}
	mysql_free_result(res);
}

void delete_mysql_agent(agent_t *mysql_agent) {
	mysql_detail_t *detail = (mysql_detail_t *)mysql_agent->detail;
	mysql_close(detail->mysql);
	free(detail);
	delete_agent(mysql_agent);
}

// TODO
MYSQL_RES *query_result(MYSQL *mysql, char *query) {
	if(mysql_query(mysql, query))
		return NULL;

	return mysql_store_result(mysql);
}