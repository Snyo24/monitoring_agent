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
#include <curl/curl.h>

size_t agent_number;

char *agent_name_list[] = {
	"mysql"
};

// TODO exception handling
void initialize() {
	// logging
	aggregator_tag = (void *)zlog_get_category("aggregator");
    if (!aggregator_tag) return;

	zlog_info(aggregator_tag, "Initialize agents");

	agent_number = sizeof(agent_name_list)/sizeof(char *);

	// Add an agents to the agent table
	agents = new_hash();
	if(!agents) {
		zlog_error(aggregator_tag, "Failed to create a agents hash");
		return;
	}

	zlog_info(aggregator_tag, "Initialize agent \'%s\'", agent_name_list[0]);
	agent_t *agent = new_mysql_agent(1, "conf/mysql.yaml");
	if(!agent) {
		zlog_error(aggregator_tag, "Failed to create an agent");
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

		for(size_t i=0; i<agent_number; ++i) {
			agent_t *agent = get_agent(agent_name_list[i]);
			zlog_debug(aggregator_tag, "Check the status of \'%s\'", agent_name_list[i]);
			if(agent->updating) {
				zlog_debug(aggregator_tag, "An agent is updating");
				if(timeup(agent)) {
					zlog_fatal(aggregator_tag, "Timeout");
					restart(agent);
				}
			} else if(outdated(agent)) {
				zlog_debug(aggregator_tag, "Outdated");
				poke(agent);
			}
		}

		zlog_debug(aggregator_tag, "Wait for %lums", TICK/1000);
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

agent_t *get_agent(char *name) {
	zlog_debug(aggregator_tag, "Get the agent \'%s\'", name);
	return hash_search(agents, name);
}