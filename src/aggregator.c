#include "aggregator.h"

#include "agent.h"
#include "mysql_agent.h"
#include "snyohash.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

void *log_tag;

size_t agent_number;

char *agent_name_list[] = {
	"mysql"
};

// TODO exception handling
void initialize() {
	// logging
	log_tag = (void *)zlog_get_category("aggregator");
    if (!log_tag) return;

	zlog_info(log_tag, "Initialize agents");

	agent_number = sizeof(agent_name_list)/sizeof(char *);

	// Add an agents to the agent table
	agents = new_hash();
	if(!agents) {
		zlog_error(log_tag, "Failed to create a agents hash");
		return;
	}

	zlog_info(log_tag, "Initialize agent \'%s\'", agent_name_list[0]);
	agent_t *agent = new_mysql_agent(1, "conf/mysql.yaml");
	if(!agent) {
		zlog_error(log_tag, "Failed to create an agent");
		delete_hash(agents);
		return;
	}

	hash_insert(agents, "mysql", agent);
	start(agent);
}

void finalize() {
	// TODO
}

void scheduler() {
	// Initiating
	initialize();

	pthread_cond_t _unused_cond = PTHREAD_COND_INITIALIZER;
	pthread_mutex_t _unused_mutex = PTHREAD_MUTEX_INITIALIZER;

	while(1) {
		timestamp loop_start = get_timestamp();

		zlog_debug(log_tag, "Check agents status");
		for(size_t i=0; i<agent_number; ++i) {
			agent_t *agent = get_agent(agent_name_list[i]);
			zlog_debug(log_tag, "Check the status of \'%s\'", agent_name_list[i]);
			if(agent->updating) {
				zlog_debug(log_tag, "An agent is updating");
				if(timeup(agent)) {
					zlog_fatal(log_tag, "Timeout");
					restart(agent);
				}
			} else if(outdated(agent)) {
				zlog_debug(log_tag, "Outdated");
				poke(agent);
			}
		}

		// TODO  WILL BE DEPRECATED
		for(size_t i=0; i<agent_number; ++i) {
			agent_t *agent = get_agent(agent_name_list[i]);
			// confirm the agents finish collecting
			if(agent->updating)
				pthread_cond_wait(&agent->synced, &agent->sync);
		}

		char json[1000];
		gather_json(json);
		zlog_debug(log_tag, "JSON: %s (%zu)", json, strlen(json));

		zlog_debug(log_tag, "Wait for %luns", TICK);
		timestamp next_loop = loop_start + TICK;
		struct timespec timeout = {
			next_loop/NANO,
			next_loop%NANO
		};
		pthread_cond_timedwait(&_unused_cond, &_unused_mutex, &timeout); // never signaled
	}

	// Finishing
	finalize();
}

void gather_json(char *json) {
	sprintf(json++, "{");
	for(size_t i=0; i<agent_number; ++i) {
		agent_t *agent = get_agent(agent_name_list[i]);
		sprintf(json, "\"%s\":", agent_name_list[i]);
		json += strlen(json);
		if(agent) {
			agent_to_json(agent, json+strlen(json), 0);
			json += strlen(json);
			if(i < agent_number-1)
				sprintf(json++, ",");
		}
	}
	sprintf(json++, "}");
}

agent_t *get_agent(char *name) {
	return hash_search(agents, name);
}