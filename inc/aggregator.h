#ifndef _AGGREGATOR_H_
#define _AGGREGATOR_H_

#include "agent.h"
#include "snyohash.h"
#include "util.h"

#define TICK NANO/4

hash_t *agents;

void initialize();
void scheduler();
void finalize();

// util
agent_t *get_agent(char *name);
void gather_json(char *json);

#endif