/**
 * @file sch.h
 * @author Snyo 
 * @brief Schedule plugins
 */

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include "runnable.h"
#include "plugin.h"

int  scheduler_init(runnable_t *sch);
void scheduler_fini(runnable_t *sch);

#endif
