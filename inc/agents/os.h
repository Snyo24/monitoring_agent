/** @file os_agent.h @author Snyo */

#ifndef _OS_H_
#define _OS_H_

#include "agent.h"

#include <mysql/mysql.h>

#define OS_AGENT_PERIOD 1

typedef struct _os_detail {
	int last_byte_in, last_byte_out;
} os_detail_t;

agent_t *os_agent_init(const char *name, const char *conf);
void collect_os_metrics(agent_t *os_agent);
void os_agent_fini(agent_t *os_agent);

#endif