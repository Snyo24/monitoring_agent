/**
 * @file scheduler.h
 * @author Snyo 
 * @brief Aggreator
 */

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include "sender.h"
#include "snyohash.h"
#include "util.h"

#define TICK NANO/4

void *scheduler_tag;

shash_t *agents;

sender_t *global_sender;

void initialize();
void schedule();
void finalize();

#endif