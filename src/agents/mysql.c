/** @file mysql_agent.c @author Snyo */

#include "agents/mysql.h"

#include "agent.h"
#include "snyohash.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#include <mysql.h>
#include <zlog.h>

char *mysql_metadata_names[] = {
	"version",
	"hostname",
	"bind_address",
	"port"
};

char *mysql_metric_names[] = {
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

MYSQL_RES *query_result(MYSQL *mysql, char *query);

// TODO exception, configuration parsing
agent_t *new_mysql_agent(const char *conf) {
	// logging
	zlog_category_t *_tag = zlog_get_category("agent_mysql");
    if (!_tag) return NULL;

    zlog_debug(_tag, "Create a new mysql agent with period %d", 1);

	agent_t *mysql_agent = new_agent(1);
	if(!mysql_agent) {
		zlog_error(_tag, "Failed to create a general agent");
		return NULL;
	}
	strncpy(mysql_agent->id, "540a14_MSQL1", 50);

	// mysql detail setup
	mysql_detail_t *detail = (mysql_detail_t *)malloc(sizeof(mysql_detail_t));
	if(!detail) {
		zlog_error(_tag, "Failed to allocate mysql detail");
		delete_agent(mysql_agent);
		return NULL;
	}

	detail->mysql = mysql_init(NULL);
	if(!detail->mysql) {
		zlog_error(_tag, "Failed to mysql_init");
		free(detail);
		delete_agent(mysql_agent);
		return NULL;
	}

	char result[4][50];
	yaml_parser(conf, (char *)result);
	if(!(mysql_real_connect(detail->mysql, result[0], result[1], result[2], NULL, 0, NULL, 0))) {
		zlog_error(_tag, "Failed to mysql_real_connect");
		mysql_close(detail->mysql);
		free(detail);
		delete_agent(mysql_agent);
		return NULL;
	}

	// logging
	mysql_agent->tag = (void *)_tag;

	// inheritance
	mysql_agent->detail = detail;

	// polymorphism
	mysql_agent->metric_number = sizeof(mysql_metric_names)/sizeof(char *);
	mysql_agent->metric_names = mysql_metric_names;
	mysql_agent->metadata_number = sizeof(mysql_metadata_names)/sizeof(char *);
	mysql_agent->metadata_names = mysql_metadata_names;

	mysql_agent->collect_metadata = collect_mysql_metadata;
	mysql_agent->collect_metrics = collect_mysql_metrics;

	mysql_agent->destructor = delete_mysql_agent;

	return mysql_agent;
}

void collect_mysql_metadata(agent_t *mysql_agent) {
	MYSQL_RES *res = query_result(((mysql_detail_t *)mysql_agent->detail)->mysql, \
		"show global variables where variable_name in (\
                                                    \'hostname\', \
                                                    \'bind_address\', \
                                                    \'port\', \
                                                    \'version\');");

	if(!res) {
		zlog_error(mysql_agent->tag, "Fail to get query result");
		exit(0);
	}

	shash_t *metadata = mysql_agent->buf[MAX_STORAGE];
	MYSQL_ROW row;
	while((row = mysql_fetch_row(res)))
		shash_insert(metadata, row[0], row[1], ELEM_STRING);

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
		zlog_error(mysql_agent->tag, "Fail to get query result");
		exit(0);
	}

	shash_t *shash = mysql_agent->buf[mysql_agent->stored++];

	MYSQL_ROW row;
	while((row = mysql_fetch_row(res))) {
		int x = atoi(row[1]);
		shash_insert(shash, row[0], &x, ELEM_INTEGER);
	}
	char buf[10000];
	shash_to_json(shash, buf);
	printf("%s\n", buf);
	mysql_free_result(res);
}

void delete_mysql_agent(agent_t *mysql_agent) {
	mysql_detail_t *detail = (mysql_detail_t *)mysql_agent->detail;
	mysql_close(detail->mysql);
	free(detail);
	delete_agent(mysql_agent);
}

MYSQL_RES *query_result(MYSQL *mysql, char *query) {
	if(mysql_query(mysql, query))
		return NULL;

	return mysql_store_result(mysql);
}