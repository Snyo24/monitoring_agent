/** @file mysql_agent.h @author Snyo */

#ifndef _MYSQL_H_
#define _MYSQL_H_

#include "agent.h"

#include <mysql/mysql.h>

#define MYSQL_AGENT_PERIOD 1

typedef struct _mysql_detail {
	MYSQL *mysql;
} mysql_detail_t;

agent_t *mysql_agent_init(const char *name, const char *conf);
void collect_mysql_metrics(agent_t *mysql_agent);
void mysql_agent_fini(agent_t *mysql_agent);

#endif