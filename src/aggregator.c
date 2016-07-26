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

size_t agent_number;

char *agent_name_list[] = {
	"mysql"
};

// TODO exception handling
void initialize() {
	agent_number = sizeof(agent_name_list)/sizeof(char *);

	// Add an agents to the agent table
	agents = new_hash();

	// TODO change parameter to conf file
	agent_t *agent = new_mysql_agent(1, "conf/mysql.yaml");
	printf("[agg] mysql agent created\n");

	start(agent);
	printf("[agg] mysql agent started\n");

	hash_insert(agents, "mysql", agent);
	printf("[agg] mysql agent inserted\n");
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
			printf("[agg] updating:%d outdated:%d timeup:%d\n", agent->updating, outdated(agent), timeup(agent));
			if(agent->updating) {
				if(timeup(agent)) {
					printf("[agg] timeup %s, do something\n", agent_name_list[i]);
					restart(agent);
				}
			} else if(outdated(agent)) {
				poke(agent);
			}
		}

		// put timestamp after collecting
		for(size_t i=0; i<agent_number; ++i) {
			agent_t *agent = get_agent(agent_name_list[i]);
			if(agent->updating) {
				// confirm the agents finish collecting
				printf("[agg] wait for synced\n");
				pthread_cond_wait(&agent->synced, &agent->sync);
				printf("[agg] synced\n");
			}
		}

		char json[1000];
		agent_t *agent = get_agent(agent_name_list[0]);
		printf("[agg] To json: waiting access lock\n");
		agent_to_json(agent, json, 0);
		printf("[agg] %s\n", json);

		timestamp next_loop = loop_start + NANO/5;
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
	return hash_search(agents, name);
}