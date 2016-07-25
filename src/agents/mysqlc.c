#include "mysqlc.h"

#include "agent.h"
#include "metric.h"
#include "snyohash.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#include <mysql.h>

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

agent_t *new_mysql_agent(int period, const char *conf) {
	// TODO exception
	agent_t *mysql_agent = new_agent(period);
	if(!mysql_agent) {
		printf("NOT ENOUGH SPACE FOR MYSQL AGENT\n");
		return NULL;
	}

	// mysql detail setup
	mysql_detail_t *detail = (mysql_detail_t *)malloc(sizeof(mysql_detail_t));
	if(!detail) {
		printf("NOT ENOUGH SPACE FOR MYSQL DETAIL\n");
		mysql_agent->delete_agent(mysql_agent);
		return NULL;
	}

	detail->mysql = mysql_init(NULL);
	if(!detail->mysql) {
		printf("mysql_init failed (mysql.c)\n");
		free(detail);
		mysql_agent->delete_agent(mysql_agent);
		exit(0);
	}

	// TODO user setup
	char host[] = "localhost";
	char user[] = "root";
	char password[] = "alcls7856@";
	char name[] = "snyo";
	if(!(mysql_real_connect(detail->mysql, host, user, password, name, 0, NULL, 0))) {
		printf("mysql_real_connect failed (mysql.c)\n");
		mysql_close(detail->mysql);
		free(detail);
		mysql_agent->delete_agent(mysql_agent);
		exit(0);
	}

	// inheritance
	mysql_agent->detail = detail;

	// polymorphism
	mysql_agent->number = sizeof(mysql_metrics)/sizeof(char *);
	mysql_agent->metrics = mysql_metrics;

	mysql_agent->thread_main = mysql_main;

	mysql_agent->collect_metadata = collect_mysql_metadata;
	mysql_agent->collect_metrics = collect_mysql_metrics;

	mysql_agent->to_json = mysql_agent_to_json;
	mysql_agent->delete_agent = delete_mysql_agent;

	return mysql_agent;
}

void *mysql_main(void *_agent) {
	printf("[mysql] started\n");
	agent_t *agent = (agent_t *)_agent;
	pthread_mutex_lock(&agent->sync);
	// The agent always holds lock "access" except waiting for alarm
	pthread_mutex_lock(&agent->access);
	printf("[mysql] collect metadata\n");
	// Collect metadata
	if(agent->collect_metadata)
		agent->collect_metadata(agent);
	pthread_cond_signal(&agent->synced);
	printf("[mysql] hold access\n");
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
		printf("query_result() failed in mysqlc.c\n");
		exit(0);
	}

	printf("[mysql] collecting metadata\n");
	hash_insert(mysql_agent->metadata, "hostname", NULL);
	hash_insert(mysql_agent->metadata, "bind_address", NULL);
	hash_insert(mysql_agent->metadata, "port", NULL);
	hash_insert(mysql_agent->metadata, "version", NULL);

	MYSQL_ROW row;
	while((row = mysql_fetch_row(res)))
		hash_insert(mysql_agent->metadata, row[0], new_metric(STRING, 0, row[1]));

	printf("[mysql] collecting metadata done\n");
	mysql_free_result(res);
}

void collect_mysql_metrics(agent_t *mysql_agent) {
	printf("[mysql] collect metric\n");
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
		printf("query_result() failed in mysqlc.c\n");
		exit(0);
	}

	hash_t *hash_table = next_storage(mysql_agent);
	for(size_t i=0; i<mysql_agent->number; ++i)
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

// TODO pretty? boolean?
void mysql_agent_to_json(agent_t *mysql_agent, char *json, int pretty) {
	sprintf(json++, "{");
	sprintf(json, "\"%s\":\"%s\",", "hostname", (char *)value(hash_search(mysql_agent->metadata, "hostname")));
	json += strlen(json);
	sprintf(json, "\"%s\":\"%s\",", "bind_address", (char *)value(hash_search(mysql_agent->metadata, "bind_address")));
	json += strlen(json);
	sprintf(json, "\"%s\":\"%s\",", "port", (char *)value(hash_search(mysql_agent->metadata, "port")));
	json += strlen(json);
	sprintf(json, "\"%s\":\"%s\",", "version", (char *)value(hash_search(mysql_agent->metadata, "version")));
	json += strlen(json);
	timestamp ts = mysql_agent->last_update - (mysql_agent->buf_stored-1)*mysql_agent->period*NANO;
	for(size_t k=0; k<mysql_agent->buf_stored; ++k) {
		hash_t *hash = fetch(mysql_agent, ts);
		sprintf(json, "\"%ld\":{", ts);
		json += strlen(json);
		for(size_t i=0; i<mysql_agent->number; ++i) {
			sprintf(json, "\"%s\":%d", mysql_agent->metrics[i], *(int *)value(hash_search(hash, mysql_agent->metrics[i])));
			json += strlen(json);
			if(i < mysql_agent->number-1)
				sprintf(json++, ",");
		}
		sprintf(json++, "}");
		if(k < mysql_agent->buf_stored-1)
			sprintf(json++, ",");
		ts += (mysql_agent->period*NANO);
	}
	sprintf(json++, "}");
}

// TODO
MYSQL_RES *query_result(MYSQL *mysql, char *query) {
	if(mysql_query(mysql, query)) {
		printf("QUERY FAIL %s\n", query);
		return NULL;
	}

	return mysql_store_result(mysql);
}