#include "agent.h"
#include "metric.h"
#include "snyohash.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

agent_t *create_agent(char *name, int period, void *(*behavior)(void *)) {
	agent_t *agent = (agent_t *)malloc(sizeof(agent_t));

	agent->name = name; // shallow copy

	agent->start_time = get_timestamp();
	agent->period = period;
	agent->last_update = 0;

	agent->collect_routine = behavior;
	pthread_mutex_init(&agent->sync, NULL);
	pthread_cond_init(&agent->synced, NULL);

	pthread_mutex_init(&agent->access, NULL);
	pthread_cond_init(&agent->alarm, NULL);

	agent->buf_start = 0;
	agent->buf_stored = 0;

	return agent;
}

int outdated(agent_t *agent) {
	return (get_timestamp() - agent->last_update)/NANO >= (agent->period);
}

// order the agent to update its metrics
// must lock access to prevent to write while reading
void update(agent_t *agent) {
	pthread_mutex_lock(&agent->access);
	pthread_cond_signal(&agent->alarm);
	pthread_mutex_unlock(&agent->access);

	// confirm the agent starts writting
	pthread_cond_wait(&agent->synced, &agent->sync);
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

void destroy_agent(agent_t *agent) {
	pthread_mutex_destroy(&agent->sync);
	pthread_mutex_destroy(&agent->access);
	pthread_cond_destroy(&agent->alarm);
	pthread_cond_destroy(&agent->synced);
	free(agent);
}

size_t agent_to_json(agent_t *agent, char *_buf) {
	char *buf = _buf;
	sprintf(buf, "\"%s\": {\n", agent->name);
	buf += strlen(buf);
	timestamp ts = agent->last_update;
	for(size_t i=0; i<agent->buf_stored; ++i) {
		hash_t *hT = fetch(agent, ts);
		if(hT) {
			sprintf(buf, "\t\"timestamp\": %lu,\n\t\"metrics\": ", ts);
			buf += strlen(buf);
			buf += (int)hash_to_json(hT, buf, (size_t (*)(void *,  char *))metric_to_json);
		}
		if(i < agent->buf_stored-1)
			sprintf(buf++, ",");
		sprintf(buf++, "\n");
		ts -= NANO * agent->period;
	}
	sprintf(buf++, "}");
	return buf-_buf;
}

hash_t *next_storage(agent_t *agent) {
	size_t idx = (agent->buf_start + agent->buf_stored)%STORAGE_SIZE;
	if(agent->buf_stored < STORAGE_SIZE) {
		agent->buf[idx] = (hash_t *)malloc(sizeof(hash_t));
		// for(size_t k=0; k<mysql_metric_number; ++k)
		// 	destroy_metric(hash_search(agent->buf[idx], mysql_metric_name_list[k]));
		// hash_destroy(agent->buf[idx]);
		// free(agent->buf[idx]);
		// agent->buf[idx] = NULL;
	}
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