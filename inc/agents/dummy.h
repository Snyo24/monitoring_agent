/** @file dummy_agent.h @author Snyo */

#ifndef _DUMMY_H_
#define _DUMMY_H_

#include "agent.h"

#include <netinet/in.h>

#define BUFFER_MAX 1024

typedef struct _dummy_detail {
	int fd;
	struct sockaddr_in server_addr;

	int contains;
	json_object *buffer[BUFFER_MAX];
} dummy_detail_t;

agent_t *new_dummy_agent(const char *name, const char *conf);
void collect_dummy_metrics(agent_t *dummy_agent);
void delete_dummy_agent(agent_t *dummy_agent);

#endif