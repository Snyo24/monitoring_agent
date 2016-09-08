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
#include <errno.h>

#include <zlog.h>
#include <curl/curl.h>
#include <json/json.h>

extern void *scheduler_tag;

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

	agent->values = json_object_new_object();

	agent->metric_array = json_object_new_array();
	
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

	free(agent);
}

int start(agent_t *agent) {
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
	struct timespec timeout = {get_timestamp()/GIGA + 2, 0};
	return pthread_cond_timedwait(&agent->synced, &agent->sync, &timeout);
}

void kill(agent_t *agent) {
	pthread_cancel(agent->running_thread);
	pthread_mutex_unlock(&agent->sync);
	pthread_mutex_unlock(&agent->access);
}

void restart(agent_t *agent) {
	zlog_fatal(scheduler_tag, "Restart \'%s\'", agent->name);
	kill(agent);
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
		agent->deadline = get_timestamp() + (GIGA/10*9)*agent->period;

		pthread_cond_signal(&agent->synced);
		pthread_mutex_unlock(&agent->sync);

		if(!agent->last_update) {
			 agent->last_update = get_timestamp();
		} else {
			agent->last_update += agent->period*GIGA;
		}
		if(!agent->first_update)
			agent->first_update = agent->last_update;

		zlog_debug(agent->tag, "Start updating");
		agent->collect_metrics(agent);
		agent->stored++;

		if(agent->stored == MAX_STORAGE) {
			zlog_debug(agent->tag, "Buffer is full");
			pack(agent);
			json_object_put(agent->values);
			agent->values = json_object_new_object();
		}
		agent->working = false;
		zlog_debug(agent->tag, "Updating done");
	}
	zlog_debug(agent->tag, "Agent is dying");
	return NULL;
}

void pack(agent_t *agent) {
	zlog_debug(agent->tag, "Packing \'%s\'", agent->name);

	json_object *package = json_object_new_object();
	extern char g_license[];
	extern char g_uuid[];
	json_object_object_add(package, "license", json_object_new_string(g_license));
	json_object_object_add(package, "uuid", json_object_new_string(g_uuid));
	json_object_object_add(package, "agent", json_object_new_string(agent->name));
	json_object *m_arr = json_object_new_array();
	for(int k=0; k<agent->metric_number; ++k) {
		json_object_array_add(m_arr, json_object_new_string(agent->metric_names[k]));
	}
	json_object_object_add(package, "metrics", m_arr);
	json_object_put(agent->metric_array);
	agent->metric_array = json_object_new_array();
	json_object_object_add(package, "values", agent->values);

	char *payload;
	// agent_to_json(agent, payload);
	payload = strdup(json_object_to_json_string(package));
	zlog_debug(agent->tag, "Payload\n%s (%zu)", payload, strlen(payload));

	zlog_debug(agent->tag, "Flushing buffer");
	agent->stored = 0;
	agent->first_update = 0;

	zlog_debug(agent->tag, "Enqueue the payload");
	enq_payload(payload);
}

void add(agent_t *agent, json_object *jarr) {
	char last_update_str[20];
	sprintf(last_update_str, "%lu", agent->last_update);
	json_object_object_add(agent->values, last_update_str, jarr);
}

bool busy(agent_t *agent) {
	return agent->working;
}

bool timeup(agent_t *agent) {
	return get_timestamp() > agent->deadline;
}

bool outdated(agent_t *agent) {
	return !agent->last_update \
	       || (get_timestamp() - agent->last_update)/GIGA >= (agent->period);
}