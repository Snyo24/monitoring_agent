#ifndef _AGENT_H_
#define _AGENT_H_

#include "snyohash.h"
#include "util.h"

#include <pthread.h>

#define STORAGE_SIZE 3

typedef struct _agent_profile {
	char *name;

	timestamp start_time; // nanosec
	timestamp last_update; // nanosec
	int period; // sec

	// Sync at the beginning of the thread run
	pthread_mutex_t sync;
	pthread_cond_t synced;

	// Thread
	void *(*collect_routine)(void *);
	pthread_t running_thread;

	// Sync for read and write
	pthread_mutex_t access;
	pthread_cond_t alarm;

	// Storage
	size_t buf_start;
	size_t buf_stored;
	hash_t *buf[STORAGE_SIZE];

	unsigned updating : 1;
} agent_t;

agent_t *create_agent(char *name, int period, void *(*behavior)(void *));

int outdated(agent_t *agent);
void update(agent_t *agent);
hash_t *fetch(agent_t *agent, timestamp ts);

void destroy_agent(agent_t *agent);

size_t agent_to_json(agent_t *agent, char *_buf);
hash_t *next_storage(agent_t *agent);

void print_agent(agent_t *agent);

#endif