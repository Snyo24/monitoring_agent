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

#include <zlog.h>
#include <curl/curl.h>

extern void *scheduler_tag;
extern sender_t *global_sender;

agent_t *agent_init(const char *name, unsigned int period) {
	char category[100];
	sprintf(category, "Agent_%s", name);
	zlog_category_t *_tag = zlog_get_category(category);
    if (!_tag) return NULL;

    zlog_debug(_tag, "Create a new agent \'%s\'", name);

	agent_t *agent = (agent_t *)malloc(sizeof(agent_t));
	if(!agent) return NULL;

	// TODO init exception
	pthread_mutex_init(&agent->sync, NULL);
	pthread_cond_init(&agent->synced, NULL);

	pthread_mutex_init(&agent->access, NULL);
	pthread_cond_init(&agent->poked, NULL);

	for(int i=0; i<MAX_STORAGE+1; ++i) {
		agent->buf[i] = shash_init();
		if(!agent->buf[i]) {
			for(; i>0; --i) shash_fini(agent->buf[i-1]);
			return NULL;
		}
	}
	
	agent->tag = (void *)_tag;
	agent->name = name;
	agent->period = period;

	return agent;
}

void agent_fini(agent_t *agent) {
	zlog_debug(scheduler_tag, "Deleting an agent");
	pthread_mutex_destroy(&agent->sync);
	pthread_cond_destroy(&agent->synced);

	pthread_mutex_destroy(&agent->access);
	pthread_cond_destroy(&agent->poked);

	// TODO free shash_elems
	for(int i=0; i<MAX_STORAGE+1; ++i)
		shash_fini(agent->buf[i]);

	free(agent);
}

void start(agent_t *agent) {
	zlog_debug(scheduler_tag, "Initialize agent values");

	agent->alive = true;
	agent->working = false;

	agent->last_update = 0;
	agent->deadline = 0;

	agent->stored = 0;

	zlog_debug(scheduler_tag, "Start agent thread");
	pthread_mutex_lock(&agent->sync);
	pthread_create(&agent->running_thread, NULL, run, agent);

	// Let the agent holds "access" lock
	pthread_cond_wait(&agent->synced, &agent->sync);
	zlog_debug(scheduler_tag, "Agent thread is now running");
}

void restart(agent_t *agent) {
	zlog_fatal(scheduler_tag, "Restart \'%s\'", agent->name);
	pthread_cancel(agent->running_thread);
	pthread_mutex_unlock(&agent->sync);
	pthread_mutex_unlock(&agent->access);
	start(agent);
}

void poke(agent_t *agent) {
	pthread_mutex_lock(&agent->access);
	zlog_info(scheduler_tag, "Poking the agent");
	pthread_cond_signal(&agent->poked);
	pthread_mutex_unlock(&agent->access);

	// confirm the agent starts collecting
	zlog_info(scheduler_tag, "Waiting to be synced");
	pthread_cond_wait(&agent->synced, &agent->sync);
}

void *run(void *_agent) {
	agent_t *agent = (agent_t *)_agent;

	pthread_mutex_lock(&agent->sync);
	pthread_mutex_lock(&agent->access);
	pthread_cond_signal(&agent->synced);
	pthread_mutex_unlock(&agent->sync);

	while(agent->alive) {
		zlog_info(agent->tag, "Waiting to be poked");
		pthread_cond_wait(&agent->poked, &agent->access);
		zlog_debug(agent->tag, "Poked");

		// Prevent the scheduler not to do other work before collecting start
		pthread_mutex_lock(&agent->sync);

		agent->working = true;
		agent->deadline = get_timestamp() + (NANO/10*9)*agent->period;

		pthread_cond_signal(&agent->synced);
		pthread_mutex_unlock(&agent->sync);

		zlog_debug(agent->tag, "Start updating");
		agent->collect_metadata(agent); // TODO, collect metadata not in every period
		agent->collect_metrics(agent);
		agent->stored++;

		if(!agent->last_update) {
			 agent->last_update = get_timestamp();
		} else {
			agent->last_update += agent->period*NANO;
		}
		if(!agent->first_update)
			agent->first_update = agent->last_update;

		if(agent->stored == MAX_STORAGE) {
			zlog_debug(agent->tag, "Buffer is full");
			pack(agent);
		}
		agent->working = false;
		zlog_debug(agent->tag, "Updating done");
	}
	zlog_debug(agent->tag, "Agent is dying");
	return NULL;
}

void pack(agent_t *agent) {
	zlog_debug(agent->tag, "Packing \'%s\'", agent->name);
	char *payload = (char *)malloc(sizeof(char) * 4196);
	agent_to_json(agent, payload);
	zlog_debug(agent->tag, "Payload\n%s (%zu)", payload, strlen(payload));

	zlog_debug(agent->tag, "Flushing buffer");
	agent->stored = 0;
	agent->first_update = 0;

	zlog_debug(agent->tag, "Enqueue the payload");
	enq_payload(payload);
}

void agent_to_json(agent_t *agent, char *json) {
	zlog_debug(agent->tag, "Dump buffer to a JSON string");
	extern char g_license[];
	extern char g_uuid[];
	json += sprintf(json, "{");
	json += sprintf(json, "\"license\":\"%s\",", g_license);
	json += sprintf(json, "\"uuid\":\"%s\",", g_uuid);
	json += sprintf(json, "\"payload\":");
	json += sprintf(json, "{");
	json += sprintf(json, "\"%s\":", agent->name);
	json += sprintf(json, "{");
	json += sprintf(json, "\"timestamp\":%lu,", agent->first_update);
	json += sprintf(json, "\"metadata\":");
	json += shash_to_json(agent->buf[MAX_STORAGE], json);
	json += sprintf(json, ",\"metrics\":");
	json += sprintf(json, "{");
	for(int i=0; i<MAX_STORAGE; ++i) {
		json += sprintf(json, "\"%d\":", i);
		json += shash_to_json(agent->buf[i], json);
		if(i < MAX_STORAGE-1)
			json += sprintf(json, ",");
	}
	json += sprintf(json, "}}}}");
}

bool busy(agent_t *agent) {
	return agent->working;
}

bool timeup(agent_t *agent) {
	return get_timestamp() > agent->deadline;
}

bool outdated(agent_t *agent) {
	return !agent->last_update \
	       || (get_timestamp() - agent->last_update)/NANO >= (agent->period);
}