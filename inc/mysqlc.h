#ifndef _MYSQLC_H_
#define _MYSQLC_H_

#include "agent.h"
#include "snyohash.h"

#include <mysql.h>

typedef struct _mysql_detail {
	MYSQL *mysql;
} mysql_detail_t;

agent_t *new_mysql_agent(int period, const char *conf);

void *mysql_main(void *_agent);
void collect_mysql_metadata(agent_t *mysql_agent);
void collect_mysql_metrics(agent_t *mysql_agent);
void delete_mysql_agent(agent_t *mysql_agent);

void mysql_agent_to_json(agent_t *mysql_agent, char *json, int pretty);
MYSQL_RES *query_result(MYSQL *mysql, char *query);

#endif