/** @file mysql_agent.c @author Snyo */

#include "agents/mysql.h"

#include "agent.h"
#include "snyohash.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#include <mysql/mysql.h>
#include <zlog.h>
#include <json/json.h>

char *mysql_metric_names[] = {
	"Queries",
	"Slow_queries",
	"Com_insert"
};

MYSQL_RES *query_result(MYSQL *mysql, char *query);

// TODO configuration parsing
agent_t *mysql_agent_init(const char *name, const char *conf) {
	agent_t *mysql_agent = agent_init(name, MYSQL_AGENT_PERIOD);
	if(!mysql_agent) {
		zlog_error(mysql_agent->tag, "Failed to create an agent");
		return NULL;
	}

	// mysql detail setup
	mysql_detail_t *detail = (mysql_detail_t *)malloc(sizeof(mysql_detail_t));
	if(!detail) {
		zlog_error(mysql_agent->tag, "Failed to allocate mysql detail");
		agent_fini(mysql_agent);
		return NULL;
	}

	detail->mysql = mysql_init(NULL);
	if(!detail->mysql) {
		zlog_error(mysql_agent->tag, "Failed to mysql_init");
		free(detail);
		agent_fini(mysql_agent);
		return NULL;
	}

	// TODO, user conf
	if(!(mysql_real_connect(detail->mysql, "localhost", "root", "snyo", NULL, 0, NULL, 0))) {
		zlog_error(mysql_agent->tag, "Failed to mysql_real_connect");
		mysql_close(detail->mysql);
		free(detail);
		agent_fini(mysql_agent);
		return NULL;
	}

	// inheritance
	mysql_agent->detail = detail;

	// polymorphism
	mysql_agent->metric_number   = sizeof(mysql_metric_names)/sizeof(char *);
	mysql_agent->metric_names    = mysql_metric_names;
	mysql_agent->collect_metrics = collect_mysql_metrics;

	mysql_agent->fini = mysql_agent_fini;

	return mysql_agent;
}

void collect_mysql_metrics(agent_t *mysql_agent) {
	zlog_debug(mysql_agent->tag, "Collecting metrics");
	MYSQL_RES *res = query_result(((mysql_detail_t *)mysql_agent->detail)->mysql, \
		"show global status where variable_name in (\'Queries\',\'Slow_queries\',\'Com_insert\');");

	if(!res) {
		zlog_error(mysql_agent->tag, "Fail to get query result");
		exit(0);
	}

	json_object *value_array = json_object_new_array();
	MYSQL_ROW row;
	while((row = mysql_fetch_row(res))) {
		int x = atoi(row[1]);
		json_object_array_add(value_array, json_object_new_int(x));
	}
	mysql_free_result(res);
	add(mysql_agent, value_array);
}

void mysql_agent_fini(agent_t *mysql_agent) {
	mysql_detail_t *detail = (mysql_detail_t *)mysql_agent->detail;
	mysql_close(detail->mysql);
	free(detail);
	agent_fini(mysql_agent);
}

MYSQL_RES *query_result(MYSQL *mysql, char *query) {
	if(mysql_query(mysql, query))
		return NULL;

	return mysql_store_result(mysql);
}