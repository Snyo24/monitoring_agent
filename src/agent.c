#include "agent.h"

#include "aggregator.h"
#include "metric.h"
#include "snyohash.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <curl/curl.h>

/*
 * Functions below are called by aggregator.
 */
agent_t *new_agent(int period) {
	agent_t *agent = (agent_t *)malloc(sizeof(agent_t));

	if(!agent)
		return NULL;
	
	agent->period = period;

	pthread_mutex_init(&agent->sync, NULL);
	pthread_cond_init(&agent->synced, NULL);

	pthread_mutex_init(&agent->access, NULL);
	pthread_cond_init(&agent->poked, NULL);

	// TODO exception handling
	// hash initiate
	agent->meta_buf = new_hash();
	for(size_t i=0; i<STORAGE_SIZE; ++i)
		agent->buf[i] = new_hash();

	return agent;
}

void start(agent_t *agent) {
	zlog_debug(aggregator_tag, "An agent is started");

	agent->alive = 1;
	agent->updating = 0;

	agent->start_time = get_timestamp();
	agent->last_update = 0;
	agent->deadline = 0;

	agent->buf_stored = 0;

	// Start the thread
	zlog_info(aggregator_tag, "Acquire sync");
	pthread_mutex_lock(&agent->sync);
	zlog_debug(aggregator_tag, "An agent is synced, starting thread");
	pthread_create(&agent->running_thread, NULL, agent->thread_main, agent);

	// Let the agent holds "access" lock
	pthread_cond_wait(&agent->synced, &agent->sync);
	zlog_debug(aggregator_tag, "Agent thread is now running");
}

// to agent
void restart(agent_t *agent) {
	zlog_fatal(aggregator_tag, "Restarting an agent");

	exit(0);
}

// to agent
int outdated(agent_t *agent) {
	zlog_debug(aggregator_tag, "Outdated?");
	return !agent->last_update \
	       || (get_timestamp() - agent->last_update)/NANO >= (agent->period);
}

// to agent
// poke the agent to update its metrics
// must lock access to prevent to write while reading
void poke(agent_t *agent) {
	zlog_info(aggregator_tag, "+-Aquire access");
	pthread_mutex_lock(&agent->access);
	zlog_info(aggregator_tag, "L Poke the agent");
	pthread_cond_signal(&agent->poked);
	zlog_info(aggregator_tag, "L Release access");
	pthread_mutex_unlock(&agent->access);

	// confirm the agent starts writting
	zlog_info(aggregator_tag, "L Release sync to be synced");
	pthread_cond_wait(&agent->synced, &agent->sync);
	zlog_debug(aggregator_tag, "+-Synced");
}

// to agent
int timeup(agent_t *agent) {
	zlog_debug(aggregator_tag, "Timeout?");
	return get_timestamp() > agent->deadline;
}

void delete_agent(agent_t *agent) {
	zlog_debug(aggregator_tag, "Deleting an agent");
	pthread_mutex_destroy(&agent->sync);
	pthread_cond_destroy(&agent->synced);

	pthread_mutex_destroy(&agent->access);
	pthread_cond_destroy(&agent->poked);

	// TODO free hash_elems
	delete_hash(agent->meta_buf);
	for(size_t i=0; i<STORAGE_SIZE; ++i)
		delete_hash(agent->buf[i]);

	free(agent);
}

/*
 * Thease functions are called by agent itself.
 */
void run(agent_t *agent) {
	while(agent->alive) {
		zlog_info(agent->log_tag, "Release access to be poked");
		pthread_cond_wait(&agent->poked, &agent->access);
		zlog_debug(agent->log_tag, "  L Poked");

		// prevent the aggregator not to do other work before collecting start
		zlog_info(agent->log_tag, "  L Acquire sync");
		pthread_mutex_lock(&agent->sync);
		agent->updating = 1;
		agent->deadline = get_timestamp()+agent->period*NANO/10*9;
		// notify collecting started to the aggregator
		zlog_debug(agent->log_tag, "  L Updating start, deadline is %lu", agent->deadline);
		zlog_info(agent->log_tag, "  L Sync with aggregator");
		pthread_cond_signal(&agent->synced);
		zlog_info(agent->log_tag, "  L Release sync");
		pthread_mutex_unlock(&agent->sync);

		agent->collect_metrics(agent);

		if(!agent->last_update) {
			 agent->last_update = get_timestamp();
		} else {
			agent->last_update += agent->period*NANO;
		}
		agent->updating = 0;
		zlog_debug(agent->log_tag, "Updating done");

		zlog_debug(agent->log_tag, "Check buffer status");
		if(buffer_full(agent)) {
			post(agent);
			flush(agent);
		}
	}
}

int buffer_full(agent_t *agent) {
	zlog_debug(agent->log_tag, "Buffer full?");
	return agent->buf_stored == STORAGE_SIZE;
}

void post(agent_t *agent) {
	zlog_debug(agent->log_tag, "POST buffer");
	char json[1500];
	to_json(agent, json, 0);
	zlog_debug(agent->log_tag, "POSTing json string:\n%s (%zu)\n", json, strlen(json));

	CURL *curl;

	curl = curl_easy_init();
	if(!curl) {
		zlog_error(agent->log_tag, "cURL init failed");
	} else {
		struct curl_slist *headers = NULL;
		headers = curl_slist_append(headers, "Accept: application/json");
		headers = curl_slist_append(headers, "Content-Type: application/json");
		headers = curl_slist_append(headers, "charsets: utf-8");

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
	for(size_t i=0; i<agent->buf_stored; ++i) {
		for(size_t k=0; k<agent->metric_number; ++k) {
			metric_t *m = (metric_t *)hash_search(agent->buf[i], agent->metrics[k]);
			if(m) delete_metric(m);
		}
		delete_hash(agent->buf[i]);
		agent->buf[i] = new_hash();
	}
	agent->buf_stored = 0;
}

// TODO pretty? boolean?
void to_json(agent_t *agent, char *json, int pretty) {
	zlog_debug(agent->log_tag, "Dump an agent to json string");
	sprintf(json++, "{");
	for(size_t k=0; k<agent->metadata_number; ++k) {
		metric_t *m = (metric_t *)hash_search(agent->meta_buf, agent->metadata_list[k]);
		if(m) {
			sprintf(json, "\"%s\":", agent->metadata_list[k]);
			json += strlen(json);
			json += metric_to_json(m, json);
			sprintf(json++, ",");
		}
	}

	for(size_t i=0; i<agent->buf_stored; ++i) {
		hash_t *hash = agent->buf[i];
		sprintf(json, "\"metrics\":{");
		json += strlen(json);
		for(size_t k=0; k<agent->metric_number; ++k) {
			metric_t *m = (metric_t *)hash_search(hash, agent->metrics[k]);
			if(m) {
				sprintf(json, "\"%s\":", agent->metrics[k]);
				json += strlen(json);
				json += metric_to_json(m, json);
				if(k < agent->metric_number-1)
					sprintf(json++, ",");
			}
		}
		sprintf(json++, "}");
		if(i < agent->buf_stored-1)
			sprintf(json++, ",");
	}
	sprintf(json++, "}");
}