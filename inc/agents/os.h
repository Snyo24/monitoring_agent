/** @file os_agent.h @author Snyo */

#ifndef _OS_H_
#define _OS_H_

#include "agent.h"

#define OS_AGENT_PERIOD 3

typedef struct _os_detail {
} os_detail_t;

agent_t *new_os_agent(const char *name, const char *conf);
void collect_os_metrics(agent_t *os_agent);
void delete_os_agent(agent_t *os_agent);

#endif