/**
 * @file agent.c
 * @author Snyo
 */
#include "agent.h"

#include "aggregator.h"
#include "metric.h"
#include "snyohash.h"
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
		agent->buf[i] = new_hash();
		if(!agent->buf[i]) {
			for(; i>0; --i)
				delete_hash(agent->buf[i-1]);
			return NULL;
		}
	}

	return agent;
}

void delete_agent(agent_t *agent) {
	zlog_debug(aggregator_tag, "Deleting an agent");
	pthread_mutex_destroy(&agent->sync);
	pthread_cond_destroy(&agent->synced);

	pthread_mutex_destroy(&agent->access);
	pthread_cond_destroy(&agent->poked);

	// TODO free hash_elems
	for(size_t i=0; i<MAX_STORAGE+1; ++i)
		delete_hash(agent->buf[i]);

	free(agent);
}

void start(agent_t *agent) {
	zlog_debug(aggregator_tag, "An agent is started");

	agent->alive = true;
	agent->updating = false;

	agent->start_time = get_timestamp();
	agent->last_update = 0;
	agent->deadline = 0;

	agent->stored = 0;

	// Start the thread
	zlog_info(aggregator_tag, "Acquire sync");
	pthread_mutex_lock(&agent->sync);
	zlog_debug(aggregator_tag, "An agent is synced, starting thread");
	pthread_create(&agent->running_thread, NULL, run, agent);

	// Let the agent holds "access" lock
	pthread_cond_wait(&agent->synced, &agent->sync);
	zlog_debug(aggregator_tag, "Agent thread is now running");
}

void *run(void *_agent) {
	agent_t *agent = (agent_t *)_agent;
	zlog_info(agent->log_tag, "Acquire sync");
	pthread_mutex_lock(&agent->sync);
	zlog_info(agent->log_tag, "Aquire access");
	pthread_mutex_lock(&agent->access);
	zlog_debug(agent->log_tag, "Agent is collecting metadata");
	agent->collect_metadata(agent);
	zlog_info(agent->log_tag, "Signal synced");
	pthread_cond_signal(&agent->synced);
	zlog_info(agent->log_tag, "Release sync");
	pthread_mutex_unlock(&agent->sync);

	while(agent->alive) {
		zlog_info(agent->log_tag, "Release access to be poked");
		pthread_cond_wait(&agent->poked, &agent->access);
		zlog_debug(agent->log_tag, "Poked");

		// prevent the aggregator not to do other work before collecting start
		zlog_info(agent->log_tag, "Acquire sync");
		pthread_mutex_lock(&agent->sync);
		agent->updating = true;
		agent->deadline = get_timestamp()+agent->period*NANO/10*9;
		// notify collecting started to the aggregator
		zlog_debug(agent->log_tag, "Updating start, deadline is %lu", agent->deadline);
		zlog_info(agent->log_tag, "Sync with aggregator");
		pthread_cond_signal(&agent->synced);
		zlog_info(agent->log_tag, "Release sync");
		pthread_mutex_unlock(&agent->sync);

		agent->collect_metrics(agent);

		if(!agent->last_update) {
			 agent->last_update = get_timestamp();
		} else {
			agent->last_update += agent->period*NANO;
		}
		agent->updating = false;
		zlog_debug(agent->log_tag, "Updating done");

		if(buffer_full(agent)) {
			post(agent);
			flush(agent);
		}
	}
	zlog_debug(agent->log_tag, "Agent is dying");
	return NULL;
}

void restart(agent_t *agent) {
	zlog_fatal(aggregator_tag, "Restarting an agent");
	// TODO
	exit(0);
}

// poke the agent to update
// must lock access to prevent to write while reading
void poke(agent_t *agent) {
	zlog_info(aggregator_tag, "Aquire access");
	pthread_mutex_lock(&agent->access);
	zlog_info(aggregator_tag, "Poke the agent");
	pthread_cond_signal(&agent->poked);
	zlog_info(aggregator_tag, "Release access");
	pthread_mutex_unlock(&agent->access);

	// confirm the agent starts writting
	zlog_info(aggregator_tag, "Release sync to be synced");
	pthread_cond_wait(&agent->synced, &agent->sync);
	zlog_debug(aggregator_tag, "Synced");
}

void post(agent_t *agent) {
	zlog_debug(agent->log_tag, "POST buffer");
	char json[1500];
	to_json(agent, json, false);
	if(!strlen(json)) {
		zlog_error(agent->log_tag, "To JSON failed");
		return;
	}
	zlog_debug(agent->log_tag, "POSTing json string:\n%s (%zu)\n", json, strlen(json));

	CURL *curl;

	curl = curl_easy_init();
	if(!curl) {
		zlog_error(agent->log_tag, "cURL init failed");
	} else {
		struct curl_slist *headers = NULL;
		headers = curl_slist_append(headers, "Accept: application/json");
		headers = curl_slist_append(headers, "Content-Type: application/json");

		curl_easy_setopt(curl, CURLOPT_URL, "http://192.168.121.14:8082");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);

	    if(curl_easy_perform(curl) != CURLE_OK)
			zlog_error(agent->log_tag, "POST failed");
 
		curl_easy_cleanup(curl);
		curl_slist_free_all(headers);
	}
}

void flush(agent_t *agent) {
	zlog_debug(agent->log_tag, "Flushing buffer");
	for(size_t i=0; i<agent->stored; ++i) {
		for(size_t k=0; k<agent->metric_number; ++k) {
			metric_t *m = (metric_t *)hash_search(agent->buf[i], agent->metric_names[k]);
			if(m) delete_metric(m);
		}
	}
	agent->stored = 0;
}

void to_json(agent_t *agent, char *json, bool pretty) {
	zlog_debug(agent->log_tag, "Dump buffer to json string");
	sprintf(json++, "{");
	for(size_t k=0; k<agent->metadata_number; ++k) {
		metric_t *m = (metric_t *)hash_search(agent->buf[MAX_STORAGE], agent->metadata_names[k]);
		if(m) {
			sprintf(json, "\"%s\":", agent->metadata_names[k]);
			json += strlen(json);
			json += metric_to_json(m, json);
			sprintf(json++, ",");
		}
	}

	for(size_t i=0; i<agent->stored; ++i) {
		hash_t *hash = agent->buf[i];
		sprintf(json, "\"metrics\":{");
		json += strlen(json);
		for(size_t k=0; k<agent->metric_number; ++k) {
			metric_t *m = (metric_t *)hash_search(hash, agent->metric_names[k]);
			if(m) {
				sprintf(json, "\"%s\":", agent->metric_names[k]);
				json += strlen(json);
				json += metric_to_json(m, json);
				if(k < agent->metric_number-1)
					sprintf(json++, ",");
			}
		}
		sprintf(json++, "}");
		if(i < agent->stored-1)
			sprintf(json++, ",");
	}
	sprintf(json++, "}");
}

bool updating(agent_t *agent) {
	zlog_debug(aggregator_tag, "Updating?");
	return agent->updating;
}

bool timeup(agent_t *agent) {
	zlog_debug(aggregator_tag, "Timeout?");
	return get_timestamp() > agent->deadline;
}

bool outdated(agent_t *agent) {
	zlog_debug(aggregator_tag, "Outdated?");
	return !agent->last_update \
	       || (get_timestamp() - agent->last_update)/NANO >= (agent->period);
}

bool buffer_full(agent_t *agent) {
	zlog_debug(agent->log_tag, "Buffer full?");
	return agent->stored == MAX_STORAGE;
}