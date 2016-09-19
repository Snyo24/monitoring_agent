/** @file network_agent.h @author Snyo */

#ifndef _NETWORK_H_
#define _NETWORK_H_

#include "agent.h"

#define NETWORK_AGENT_PERIOD 3

typedef struct _network_detail {
} network_detail_t;

agent_t *new_network_agent(const char *name, const char *conf);
void collect_network_metrics(agent_t *network_agent);
void delete_network_agent(agent_t *network_agent);

#endif