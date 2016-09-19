/** @file dummy_agent.c @author Snyo */

#include "agents/dummy.h"

#include "agent.h"
#include "snyohash.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#include <netinet/in.h>
#include <zlog.h>
#include <json/json.h>

const char *dummy_metric_names[] = {
};

// TODO configuration parsing
agent_t *new_dummy_agent(const char *name, const char *conf) {
	agent_t *dummy_agent = new_agent(name, 3);
	if(!dummy_agent) {
		zlog_error(dummy_agent->tag, "Failed to create an agent");
		return NULL;
	}

	// dummy detail setup
	dummy_detail_t *detail = (dummy_detail_t *)malloc(sizeof(dummy_detail_t));
	if(!detail) {
		zlog_error(dummy_agent->tag, "Failed to allocate dummy detail");
		delete_agent(dummy_agent);
		return NULL;
	}
	if((detail->fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		zlog_error(dummy_agent->tag, "Failed to allocate dummy detail");
		delete_agent(dummy_agent);
		return NULL;
	}
	memset(&detail->server_addr, 0, sizeof(struct sockaddr_in));
	detail->server_addr.sin_family = AF_INET;
	detail->server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	detail->server_addr.sin_port = htons(7777);

	if(bind(detail->fd, (struct sockaddr *)&detail->server_addr, sizeof(struct sockaddr)) < 0) {
		zlog_error(dummy_agent->tag, "Failed to allocate dummy detail");
		delete_agent(dummy_agent);
		return NULL;
	}

	detail->contains = 0;

	// inheritance
	dummy_agent->detail = detail;

	// polymorphism
	dummy_agent->metric_names    = dummy_metric_names;
	dummy_agent->collect_metrics = collect_dummy_metrics;

	dummy_agent->fini = delete_dummy_agent;

	return dummy_agent;
}

void collect_dummy_metrics(agent_t *dummy_agent) {
	zlog_debug(dummy_agent->tag, "Collecting metrics");
	json_object *values = json_object_new_array();
	dummy_detail_t *dummy_detail = (dummy_detail_t *)dummy_agent->detail;
	char data[2048];
	struct sockaddr from;
	unsigned int len;
	recvfrom(dummy_detail->fd, data, 2048-1, 0, &from, &len);
	add_metrics(dummy_agent, values);
}

void delete_dummy_agent(agent_t *dummy_agent) {
	free(dummy_agent->detail);
	delete_agent(dummy_agent);
}