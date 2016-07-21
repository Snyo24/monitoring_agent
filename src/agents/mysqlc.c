#include "mysqlc.h"

#include "snyohash.h"
#include "metric.h"
#include "agent.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#include <mysql.h>

size_t mysql_metric_number;

char *mysql_metric_name_list[] = {
	"mysql/version",
	"mysql/queries[number]",
	"mysql/questions[number]",
	"mysql/open_tables[number]",
	"mysql/slow_queries[number]",
	"mysql/uptime[second]"
};

void collect_mysql(agent_t *agent, MYSQL *mysql) {
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
		hash_init(metric_table, mysql_metric_number);
		char tmp[100];
		getVersion(tmp, mysql);
		hash_insert(metric_table, "mysql/version", create_metric(STRING, 0, tmp));
		getQueries(tmp, mysql);
		int x = atoi(tmp);
		hash_insert(metric_table, "mysql/queries[number]", create_metric(INTEGER, "s", &x));
		getQuestions(tmp, mysql);
		x = atoi(tmp);
		hash_insert(metric_table, "mysql/questions[number]", create_metric(INTEGER, "s", &x));
		getOpenTables(tmp, mysql);
		x = atoi(tmp);
		hash_insert(metric_table, "mysql/open_tables[number]", create_metric(INTEGER, "s", &x));
		getSlowQueries(tmp, mysql);
		x = atoi(tmp);
		hash_insert(metric_table, "mysql/slow_queries[number]", create_metric(INTEGER, "s", &x));
		getUptime(tmp, mysql);
		x = atoi(tmp);
		hash_insert(metric_table, "mysql/uptime[second]", create_metric(INTEGER, "s", &x));

		pthread_mutex_lock(&agent->sync);
		// notify collecting done to the aggregator 
		pthread_cond_signal(&agent->synced);
		pthread_mutex_unlock(&agent->sync);
	}
}

void *mysql_routine(void *_agent) {
	MYSQL *mysql, con;

	mysql_metric_number = sizeof(mysql_metric_name_list)/sizeof(char *);
	mysql_init(&con);
	if(!(mysql = mysql_real_connect(&con, DB_HOST, DB_USER, DB_PASS, DB_NAME, 0, NULL, 0))) {
		//TODO
		exit(0);
	}
	collect_mysql(_agent, mysql);
	mysql_close(&con);
	return NULL;
}

void getVersion(void *buf, MYSQL *mysql) {
	getResultByQuery(buf, "SHOW VARIABLES WHERE VARIABLE_NAME=\'VERSION\';", mysql);
}

void getOpenTables(void *buf, MYSQL *mysql) {
	getResultByQuery(buf, "SHOW STATUS WHERE VARIABLE_NAME=\'OPEN_TABLES\';", mysql);
}

void getQueries(void *buf, MYSQL *mysql) {
	getResultByQuery(buf, "SHOW STATUS WHERE VARIABLE_NAME=\'QUERIES\';", mysql);
}

void getQuestions(void *buf, MYSQL *mysql) {
	getResultByQuery(buf, "SHOW STATUS WHERE VARIABLE_NAME=\'QUESTIONS\';", mysql);
}

void getSlowQueries(void *buf, MYSQL *mysql) {
	getResultByQuery(buf, "SHOW STATUS WHERE VARIABLE_NAME=\'SLOW_QUERIES\';", mysql);
}

void getUptime(void *buf, MYSQL *mysql) {
	getResultByQuery(buf, "SHOW STATUS WHERE VARIABLE_NAME=\'UPTIME\';", mysql);
}

size_t getResultByQuery(void *buf, char *query, MYSQL *mysql) {
	if(mysql_query(mysql, query)) {
		printf("FAIL %s\n", query);
		((char *)buf)[0] = '\0';
		return 0;
	}

	MYSQL_RES *res = mysql_store_result(mysql);
	MYSQL_ROW row;
	while((row = mysql_fetch_row(res)))
		sprintf(buf, "%s", row[1]);
	return strlen(buf);
}