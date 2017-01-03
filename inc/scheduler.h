/**
 * @file scheduler.h
 * @author Snyo 
 * @brief Schedule plugins
 */

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include "routine.h"

int  scheduler_init(routine_t *sch);
void scheduler_fini(routine_t *sch);

#endif
