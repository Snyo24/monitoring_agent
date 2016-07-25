#ifndef _AGENT_H_
#define _AGENT_H_

#include "snyohash.h"
#include "util.h"

#include <pthread.h>

#define STORAGE_SIZE 5

typedef struct _agent_info agent_t;

struct _agent_info {
	/* Status variables */
	volatile unsigned alive : 1;
	volatile unsigned updating : 1;

	/* Agent info */
	int       period;

	timestamp start_time;
	timestamp last_update;
	timestamp deadline;

	/* Thread variables */
	void            *(*thread_main)(void *);
	pthread_t       running_thread;

	pthread_mutex_t sync;   // Synchronization
	pthread_cond_t  synced;

	pthread_mutex_t access; // Read, Write
	pthread_cond_t  poked;

	/* Metric info */
	int number;
	char **metrics;

	/* Buffer */
	hash_t *metadata;

	size_t buf_start;
	size_t buf_stored;
	hash_t *buf[STORAGE_SIZE]; // metric hash
	
	/* Inheritance */
	void *detail;

	/* Polymrphism */
	void (*collect_metadata)(agent_t *);
	void (*collect_metrics)(agent_t *);
	void (*to_json)(agent_t *, char *, int);
	void (*delete_agent)(agent_t *);
};

agent_t *new_agent(int period);

void start(agent_t *agent);
void restart(agent_t *agent);
void run(agent_t *agent);	
int outdated(agent_t *agent);
void poke(agent_t *agent);
int timeup(agent_t *agent);

void delete_agent(agent_t *agent);

hash_t *fetch(agent_t *agent, timestamp ts);
hash_t *next_storage(agent_t *agent);

void print_agent(agent_t *agent);

#endif