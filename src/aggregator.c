#include "aggregator.h"

#include "agent.h"
#include "mysqlc.h"
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

void initialize() {
	agent_number = sizeof(agent_name_list)/sizeof(char *);

	// Add an agents to the agent table
	hash_init(&agents, agent_number);
	hash_insert(&agents, "mysql", create_agent("mysql", 1, mysql_routine));

	for(size_t i=0; i<agent_number; ++i) {
		agent_t *agent = get_agent(agent_name_list[i]);
		// Start the thread
		pthread_mutex_lock(&agent->sync);
		pthread_create(&(agent->running_thread), NULL, agent->collect_routine, agent);

		// Let the agent holds "access" lock
		pthread_cond_wait(&agent->synced, &agent->sync);
	}
}

void finalize() {
	for(size_t i=0; i<agent_number; ++i)
		destroy_agent(get_agent(agent_name_list[i]));
	hash_destroy(&agents, (void (*)(void *))destroy_agent);
}

void scheduler() {
	// Initiating
	initialize();

	struct timespec timeout;
	pthread_cond_t _unused_cond = PTHREAD_COND_INITIALIZER;
	pthread_mutex_t _unused_mutex = PTHREAD_MUTEX_INITIALIZER;

	while(1) {
		timestamp loop_start = get_timestamp();

		for(size_t i=0; i<agent_number; ++i) {
			agent_t *agent = get_agent(agent_name_list[i]);
			if(outdated(agent)) {
				update(agent);
				// TODO do we need this variable?
				agent->updating = 1;
				// confirm the agents finish collecting
				// pthread_cond_wait(&agent->synced, &agent->sync);
				// if(!agent->last_update)
				// 	 agent->last_update = get_timestamp();
				// else
				// 	agent->last_update += agent->period*NANO;
			}
		}

		// put timestamp after collecting
		for(size_t i=0; i<agent_number; ++i) {
			agent_t *agent = get_agent(agent_name_list[i]);
			if(agent->updating) {
				// print_agent(agent);
				// confirm the agents finish collecting
				pthread_cond_wait(&agent->synced, &agent->sync);
				if(!agent->last_update)
					 agent->last_update = get_timestamp();
				else
					agent->last_update += agent->period*NANO;

				// TODO timeout with pthread_cond_timewait
				// timestamp collect_limit = loop_start;
				// collect_limit += (agent->period*NANO)/10*8;
				// timeout.tv_sec = collect_limit/NANO;
				// timeout.tv_nsec = collect_limit%NANO;
				// printf("%lu\n", loop_start);
				// printf("%lu\n", get_timestamp());
				// printf("time limit: %lu%lu\n", timeout.tv_sec, timeout.tv_nsec);
				// if(pthread_cond_timedwait(&agent->synced, &agent->sync, &timeout) == ETIMEDOUT)
				// 	printf("TIMEDOUT\n");
				// else {
				// 	if(!agent->last_update)
				// 		 agent->last_update = get_timestamp();
				// 	else
				// 		agent->last_update += agent->period*NANO;
				// }
				// printf("%lu\n", loop_start);
				// printf("%lu\n", get_timestamp());
				// printf("%lu\n", collect_limit);
				agent->updating = 0;
			}
		}

		// char b[1000];
		// timestamp fetch_ofs = 0 * NANO;
		// agent_t *agent = get_agent(agent_name_list[0]);
		// hash_t *hT = fetch(agent, agent->last_update-fetch_ofs);
		// if(hT) {
		// 	int n = hash_to_json(hT, b, (void(*)(char *, void *))metric_to_json);
		// 	printf("\"%lu\": %.*s\n\n", agent->last_update-fetch_ofs, n, b);
		// }
		// make_payload();

		timestamp next_loop = loop_start + NANO/10;
		timeout.tv_sec = next_loop/NANO;
		timeout.tv_nsec = next_loop%NANO;
		pthread_cond_timedwait(&_unused_cond, &_unused_mutex, &timeout); // never signaled

		agent_t *agent = get_agent("mysql");
		char b[1000000];
		printf("%.*s\n", (int)agent_to_json(agent, b), b);
	}

	// Finishing
	finalize();
}

agent_t *get_agent(char *name) {
	return hash_search(&agents, name);
}