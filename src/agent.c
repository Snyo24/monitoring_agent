/**
 * @file agent.c
 * @author Snyo
 */
#include "agent.h"

#include "scheduler.h"
#include "snyohash.h"
#include "sender.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>

#include <curl/curl.h>

agent_t *new_agent(unsigned int period) {
	agent_t *agent = (agent_t *)malloc(sizeof(agent_t));

	if(!agent)
		return NULL;
	
	agent->period = period;

	// TODO init exception
	pthread_mutex_init(&agent->sync, NULL);
	pthread_cond_init(&agent->synced, NULL);

	pthread_mutex_init(&agent->access, NULL);
	pthread_cond_init(&agent->poked, NULL);

	for(size_t i=0; i<MAX_STORAGE+1; ++i) {
		agent->buf[i] = new_shash();
		if(!agent->buf[i]) {
			for(; i>0; --i)
				delete_shash(agent->buf[i-1]);
			return NULL;
		}
	}

	return agent;
}

void delete_agent(agent_t *agent) {
	zlog_debug(scheduler_tag, "Deleting an agent");
	pthread_mutex_destroy(&agent->sync);
	pthread_cond_destroy(&agent->synced);

	pthread_mutex_destroy(&agent->access);
	pthread_cond_destroy(&agent->poked);

	// TODO free shash_elems
	for(size_t i=0; i<MAX_STORAGE+1; ++i)
		delete_shash(agent->buf[i]);

	free(agent);
}

void start(agent_t *agent) {
	zlog_debug(scheduler_tag, "An agent is started");

	agent->alive = true;
	agent->updating = false;

	agent->last_update = 0;
	agent->deadline = 0;

	agent->stored = 0;

	// Start the thread
	zlog_info(scheduler_tag, "Acquire sync");
	pthread_mutex_lock(&agent->sync);
	zlog_debug(scheduler_tag, "An agent is synced, starting thread");
	pthread_create(&agent->running_thread, NULL, run, agent);

	// Let the agent holds "access" lock
	pthread_cond_wait(&agent->synced, &agent->sync);
	zlog_debug(scheduler_tag, "Agent thread is now running");
}

void *run(void *_agent) {
	agent_t *agent = (agent_t *)_agent;
	zlog_info(agent->tag, "Acquire sync");
	pthread_mutex_lock(&agent->sync);
	zlog_info(agent->tag, "Aquire access");
	pthread_mutex_lock(&agent->access);
	zlog_debug(agent->tag, "Agent is collecting metadata");
	agent->collect_metadata(agent);
	zlog_info(agent->tag, "Signal synced");
	pthread_cond_signal(&agent->synced);
	zlog_info(agent->tag, "Release sync");
	pthread_mutex_unlock(&agent->sync);

	while(agent->alive) {
		zlog_info(agent->tag, "Release access to be poked");
		pthread_cond_wait(&agent->poked, &agent->access);
		zlog_debug(agent->tag, "Poked");

		// prevent the scheduler not to do other work before collecting start
		zlog_info(agent->tag, "Acquire sync");
		pthread_mutex_lock(&agent->sync);
		agent->updating = true;
		agent->deadline = get_timestamp()+agent->period*NANO/10*9;
		// notify collecting started to the scheduler
		zlog_debug(agent->tag, "Updating start, deadline is %lu", agent->deadline);
		zlog_info(agent->tag, "Sync with scheduler");
		pthread_cond_signal(&agent->synced);
		zlog_info(agent->tag, "Release sync");
		pthread_mutex_unlock(&agent->sync);

		agent->collect_metrics(agent);

		if(!agent->last_update) {
			 agent->first_update = get_timestamp();
			 agent->last_update = agent->first_update;
		} else {
			agent->last_update += agent->period*NANO;
		}
		agent->updating = false;
		zlog_debug(agent->tag, "Updating done");

		if(buffer_full(agent)) {
			post(agent);
			flush(agent);
		}
	}
	zlog_debug(agent->tag, "Agent is dying");
	return NULL;
}

void restart(agent_t *agent) {
	zlog_fatal(scheduler_tag, "Restarting an agent");
	// TODO
	exit(0);
}

// poke the agent to update
// must lock access to prevent to write while reading
void poke(agent_t *agent) {
	zlog_info(scheduler_tag, "Aquire access");
	pthread_mutex_lock(&agent->access);
	zlog_info(scheduler_tag, "Poke the agent");
	pthread_cond_signal(&agent->poked);
	zlog_info(scheduler_tag, "Release access");
	pthread_mutex_unlock(&agent->access);

	// confirm the agent starts writting
	zlog_info(scheduler_tag, "Release sync to be synced");
	pthread_cond_wait(&agent->synced, &agent->sync);
	zlog_debug(scheduler_tag, "Synced");
}

void post(agent_t *agent) {
	zlog_debug(agent->tag, "POST data");
	char payload[10000];
	agent_to_json(agent, payload);
	zlog_debug(agent->tag, "Payload\n%s (%zu)", payload, strlen(payload));
	// register_json(global_sender, json);
}

void flush(agent_t *agent) {
	zlog_debug(agent->tag, "Flushing buffer");
	agent->stored = 0;
}

void agent_to_json(agent_t *agent, char *json) {
	zlog_debug(agent->tag, "Dump buffer to a JSON string");
	json += sprintf(json, "{");
	json += sprintf(json, "\"last_update\":%lu,", agent->last_update);
	json += sprintf(json, "\"period\":%d,", agent->period);
	json += sprintf(json, "\"metadata\":");
	json += shash_to_json(agent->buf[MAX_STORAGE], json);
	json += sprintf(json, ",\"metrics\":{");
	for(size_t i=0; i<MAX_STORAGE; ++i) {
		json += sprintf(json, "\"%zu\":", MAX_STORAGE-i-1);
		json += shash_to_json(agent->buf[i], json);
		if(i < MAX_STORAGE-1)
			json += sprintf(json, ",");
	}
	json += sprintf(json, "}");
	json += sprintf(json, "}");
}

bool updating(agent_t *agent) {
	zlog_debug(scheduler_tag, "Updating?");
	return agent->updating;
}

bool timeup(agent_t *agent) {
	zlog_debug(scheduler_tag, "Timeout?");
	return get_timestamp() > agent->deadline;
}

bool outdated(agent_t *agent) {
	zlog_debug(scheduler_tag, "Outdated?");
	return !agent->last_update \
	       || (get_timestamp() - agent->last_update)/NANO >= (agent->period);
}

bool buffer_full(agent_t *agent) {
	zlog_debug(agent->tag, "Buffer full?");
	return agent->stored == MAX_STORAGE;
}

agent_t *get_agent(char *name) {
	zlog_debug(scheduler_tag, "Get the agent \'%s\'", name);
	return shash_search(agents, name);
}