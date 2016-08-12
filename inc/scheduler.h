/**
 * @file scheduler.h
 * @author Snyo 
 * @brief Schedule agents
 */

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include "sender.h"
#include "snyohash.h"
#include "util.h"

#define TICK NANO/4

void scheduler_init();
void schedule();
void scheduler_fini();

#endif