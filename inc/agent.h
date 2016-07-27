#ifndef _AGENT_H_
#define _AGENT_H_

#include "snyohash.h"
#include "util.h"

#include <pthread.h>

#define STORAGE_SIZE 3

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
	int metric_number;
	char **metrics;
	int metadata_number;
	char **metadata_list;

	/* Buffer */
	hash_t *meta_buf;
	size_t buf_stored;
	hash_t *buf[STORAGE_SIZE]; // metric hash

	/* Logging */
	void *log_tag;
	
	/* Inheritance */
	void *detail;

	/* Polymrphism */
	void (*collect_metadata)(agent_t *);
	void (*collect_metrics)(agent_t *);
	void (*destructor)(agent_t *);
};

agent_t *new_agent(int period);
void delete_agent(agent_t *agent);

// Command to agents
void start(agent_t *agent);
void restart(agent_t *agent);
int outdated(agent_t *agent);
void poke(agent_t *agent);
int timeup(agent_t *agent);

int buffer_full(agent_t *agent);
void post(agent_t *agent);
void flush(agent_t *agent);
void to_json(agent_t *mysql_agent, char *json, int pretty);

// Agent behavior
void run(agent_t *agent);

#endif