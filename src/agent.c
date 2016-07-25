#include "agent.h"
#include "metric.h"
#include "snyohash.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

agent_t *new_agent(int period) {
	agent_t *agent = (agent_t *)malloc(sizeof(agent_t));

	if(!agent)
		return NULL;
	
	agent->period = period;

	pthread_mutex_init(&agent->sync, NULL);
	pthread_cond_init(&agent->synced, NULL);

	pthread_mutex_init(&agent->access, NULL);
	pthread_cond_init(&agent->poked, NULL);

	// TODO automate sizing, exception handling
	// hash initiate
	agent->metadata = new_hash();
	for(size_t i=0; i<STORAGE_SIZE; ++i)
		agent->buf[i] = new_hash();

	return agent;
}

void sig_alarm_handler(int SIGNUM) {
	printf("%d\n", SIGNUM);
}

void start(agent_t *agent) {
	agent->alive = 1;
	agent->updating = 0;

	agent->start_time = get_timestamp();
	agent->last_update = 0;
	agent->deadline = 0;

	agent->buf_start = 0;
	agent->buf_stored = 0;

	// Start the thread
	pthread_mutex_lock(&agent->sync);
	printf("[agent] get sync, start thread\n");
	pthread_create(&agent->running_thread, NULL, agent->thread_main, agent);

	// Let the agent holds "access" lock
	pthread_cond_wait(&agent->synced, &agent->sync);
	printf("[agent] thread is now running\n");
}

void restart(agent_t *agent) {
	printf("Restarting..\n");

	pthread_kill(agent->running_thread, SIGALRM);

	pthread_mutex_destroy(&agent->sync);
	pthread_mutex_destroy(&agent->access);

	pthread_mutex_init(&agent->sync, NULL);
	pthread_mutex_init(&agent->access, NULL);

	start(agent);
	run(agent);
}

void run(agent_t *agent) {
	while(agent->alive) {
		printf("[agent] release access lock, waiting poked\n");
		pthread_cond_wait(&agent->poked, &agent->access);
		printf("[agent] got poked\n");

		pthread_mutex_lock(&agent->sync);
		agent->updating = 1;
		agent->deadline = get_timestamp()+agent->period*NANO/10 *9;
		printf("[agent] update the deadline to %lu\n", agent->deadline);
		// notify collecting started to the aggregator
		pthread_cond_signal(&agent->synced);
		pthread_mutex_unlock(&agent->sync);

		agent->collect_metrics(agent);

		pthread_mutex_lock(&agent->sync);
		printf("[agent] the agent finishes collecting\n");
		if(!agent->last_update) {
			 agent->last_update = get_timestamp();
		} else {
			agent->last_update += agent->period*NANO;
		}
		printf("[agent] update the last_update to %lu\n", agent->last_update);
		agent->updating = 0;
		// notify collecting done to the aggregator 
		pthread_cond_signal(&agent->synced);
		pthread_mutex_unlock(&agent->sync);
		printf("[agent] synced\n");
	}
}

int outdated(agent_t *agent) {
	return !agent->last_update \
	       || (get_timestamp() - agent->last_update)/NANO >= (agent->period);
}

// order the agent to update its metrics
// must lock access to prevent to write while reading
void poke(agent_t *agent) {
	pthread_mutex_lock(&agent->access);
	printf("[agent] poke agent\n");
	pthread_cond_signal(&agent->poked);
	pthread_mutex_unlock(&agent->access);

	// confirm the agent starts writting
	pthread_cond_wait(&agent->synced, &agent->sync);
}

int timeup(agent_t *agent) {
	return get_timestamp() > agent->deadline;
}

void delete_agent(agent_t *agent) {
	pthread_mutex_destroy(&agent->sync);
	pthread_cond_destroy(&agent->synced);

	pthread_mutex_destroy(&agent->access);
	pthread_cond_destroy(&agent->poked);

	// TODO free hash_elems
	delete_hash(agent->metadata);
	for(size_t i=0; i<STORAGE_SIZE; ++i)
		delete_hash(agent->buf[i]);

	free(agent);
}

// Guarantee that no writting while fetching
hash_t *fetch(agent_t *agent, timestamp ts) {
	pthread_mutex_lock(&agent->access);
	if(!agent->last_update || ts < agent->start_time || ts > agent->last_update) {
		printf("DATA NOT COLLCTED ON TIMESTAMP %lu\n", ts);
		pthread_mutex_unlock(&agent->access);
		return NULL;
	}
	int elapsed = (agent->last_update-ts)/NANO;
	if(elapsed%agent->period != 0) {
		printf("NO DATA ON TIMESTAMP %lu\n", ts);
		pthread_mutex_unlock(&agent->access);
		return NULL;
	}
	size_t ofs = elapsed/agent->period;
	if(ofs >= agent->buf_stored) {
		printf("TOO OLD DATA TO FETCH ON TIMESTAMP %lu\n", ts);
		pthread_mutex_unlock(&agent->access);
		return NULL;
	}
	size_t index = (agent->buf_start+agent->buf_stored-ofs-1)%STORAGE_SIZE;
	hash_t *hT = agent->buf[index];
	pthread_mutex_unlock(&agent->access);
	return hT;
}

hash_t *next_storage(agent_t *agent) {
	size_t idx = (agent->buf_start + agent->buf_stored)%STORAGE_SIZE;
	if(agent->buf_stored == STORAGE_SIZE)
		agent->buf_start = (agent->buf_start+1)%STORAGE_SIZE;
	else
		agent->buf_stored++;
	return agent->buf[idx];
}

void print_agent(agent_t *agent) {
	printf("%s %lu\n", "START TIME:  ", agent->start_time);
	printf("%s %lu\n", "LAST UPDATE: ", agent->last_update);
	printf("%s %d\n", "PERIOD:      ", agent->period);
	printf("%s %zu ", "STORED:      ", agent->buf_stored);
	printf("%s\n", agent->buf_stored==STORAGE_SIZE? " (FULL)": "");
}