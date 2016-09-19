/** @file mysql_agent.h @author Snyo */

#ifndef _MYSQL_H_
#define _MYSQL_H_

#include "agent.h"

#include <mysql/mysql.h>

#define MYSQL_AGENT_PERIOD 3

typedef struct _mysql_detail {
	MYSQL *mysql;
} mysql_detail_t;

agent_t *new_mysql_agent(const char *name, const char *conf);
void collect_mysql_metrics(agent_t *mysql_agent);
void delete_mysql_agent(agent_t *mysql_agent);

#endif