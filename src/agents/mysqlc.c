#include "mysqlc.h"

#include "snyohash.h"
#include "metric.h"
#include "agent.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#include <mysql.h>

size_t mysql_metric_number;

char *mysql_metric_list[] = {
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
	"Com_prepare_sql"
};

void collect_mysql(MYSQL *mysql, agent_t *agent) {
	get_mysql_metadata(mysql, &agent->metadata);

	pthread_mutex_lock(&agent->sync);
	// The agent always holds lock "access" except waiting for alarm
	pthread_mutex_lock(&agent->access);
	pthread_cond_signal(&agent->synced);
	pthread_mutex_unlock(&agent->sync);
	while(1) {
		pthread_cond_wait(&agent->alarm, &agent->access);

		pthread_mutex_lock(&agent->sync);
		// notify collecting started to the aggregator 
		pthread_cond_signal(&agent->synced);
		pthread_mutex_unlock(&agent->sync);

		hash_t *metric_table = next_storage(agent);

		get_CRUD_statistics(mysql, metric_table);

		pthread_mutex_lock(&agent->sync);
		// notify collecting done to the aggregator 
		pthread_cond_signal(&agent->synced);
		pthread_mutex_unlock(&agent->sync);
	}
}

void *mysql_routine(void *_agent) {
	mysql_metric_number = sizeof(mysql_metric_list)/sizeof(char *);

	MYSQL *mysql = mysql_init(NULL);
	if(!mysql){
		printf("mysql_init failed (mysql.c)\n");
		exit(0);
	}
	if(!(mysql_real_connect(mysql, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0))) {
		//TODO
		printf("mysql_real_connect failed (mysql.c)\n");
		exit(0);
	}
	collect_mysql(mysql, _agent);
	mysql_close(mysql);
	return NULL;
}

void get_mysql_metadata(MYSQL *mysql, hash_t *hash_table) {
	MYSQL_RES *res = query_result(mysql, \
		"show global variables where variable_name in (\
													\'hostname\', \
													\'bind_address\', \
													\'port\', \
													\'version\');");

	if(!res) {
		printf("bb\n");
		exit(0);
	}

	hash_insert(hash_table, "hostname", NULL);
	hash_insert(hash_table, "bind_address", NULL);
	hash_insert(hash_table, "port", NULL);
	hash_insert(hash_table, "version", NULL);

	MYSQL_ROW row;
	while((row = mysql_fetch_row(res))) {
		hash_insert(hash_table, row[0], create_metric(STRING, 0, row[1]));
	}
	mysql_free_result(res);
}

void get_CRUD_statistics(MYSQL *mysql, hash_t *hash_table) {
	MYSQL_RES *res = query_result(mysql, \
		"show global status where variable_name in (\
													\'Com_delete\', \
													\'Com_delete_multi\', \
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
													\'Com_prepare_sql\');");

	if(!res) {
		printf("bb\n");
		exit(0);
	}

	for(size_t i=0; i<mysql_metric_number; ++i)
		hash_insert(hash_table, mysql_metric_list[i], NULL);

	MYSQL_ROW row;
	while((row = mysql_fetch_row(res))) {
		int x = atoi(row[1]);
		hash_insert(hash_table, row[0], create_metric(INTEGER, 0, &x));
	}
	mysql_free_result(res);
}

MYSQL_RES *query_result(MYSQL *mysql, char *query) {
	if(mysql_query(mysql, query)) {
		printf("QUERY FAIL %s\n", query);
		return NULL;
	}

	return mysql_store_result(mysql);
}