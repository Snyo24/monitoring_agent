/** @file agent.h @author Snyo */

#ifndef _AGENT_H_
#define _AGENT_H_

#include "snyohash.h"
#include "util.h"

#include <stdbool.h>
#include <pthread.h>
#include <json/json.h>

#define MAX_STORAGE 2

typedef struct _agent agent_t;

struct _agent {
	/* Status variables */
	volatile unsigned alive : 1;
	volatile unsigned working : 1;

	/* Agent info */
	const char   *name;
	unsigned int period;
	timestamp    first_update;
	timestamp    last_update;
	timestamp    deadline;

	/* Thread variables */
	pthread_t       running_thread;
	pthread_mutex_t sync;   // Synchronization
	pthread_cond_t  synced;
	pthread_mutex_t access; // Read, Write
	pthread_cond_t  poked;

	/* Buffer */
	int     stored;
	shash_t *buf[MAX_STORAGE+1]; // +1 for metadata

	/* Logging */
	void *tag;

	/* Metric info */
	int  metric_number;
	char **metric_names;
	int  metadata_number;
	char **metadata_names;
	
	/* Inheritance */
	void *detail;

	/* Polymorphism */
	void (*collect_metadata)(agent_t *);
	void (*collect_metrics)(agent_t *);
	void (*fini)(agent_t *);
};

/** @brief Constructor */
agent_t *agent_init(const char *name, unsigned int period);
/** @brief Destruct2or */
void agent_fini(agent_t *agent);

/**
 * @defgroup agent_syscall
 * System Calls
 * @{
 */
/** @brief Start the agent in thread */
int start(agent_t *agent);
void kill(agent_t *agent);
/** @brief Run loop */
void *run(void *_agent);
/** @brief Restart the agent */
void restart(agent_t *agent);
/** @brief Poke the agent to start update */
void poke(agent_t *agent);
/** @brief Post the buffer */
void pack(agent_t *agent);
/** @brief Make the buffer to a JSON string */
void agent_to_json(agent_t *agent, char *json);
/** @} */

/**
 * @defgroup agent_check
 * Check status of the agent
 * @{
 */
bool busy(agent_t *agent);
bool timeup(agent_t *agent);
bool outdated(agent_t *agent);

#endif