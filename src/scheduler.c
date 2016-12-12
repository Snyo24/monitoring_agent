/**
 * @file scheduler.c
 * @author Snyo
 */

#include "scheduler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include <zlog.h>

#include "target.h"
#include "util.h"

#define SCHEDULER_TICK 0.5

typedef struct target_set_t {
	unsigned up;
} target_set_t;

//static target_set_t target_set = {0};

//static int  _indexing();
//static void _deindexing();

int scheduler_init(scheduler_t *scheduler) {
	if(runnable_init((runnable_t *)&scheduler) < 0)
		return -1;

	scheduler->tag = zlog_get_category("scheduler");
	if(!scheduler->tag);

	DEBUG(zlog_debug(scheduler->tag, "Initailize a target table"));
	memset(scheduler->targets, 0, sizeof(target_t *)*MAX_PLUGIN);
    scheduler->targetc = 0;

	scheduler->period = SCHEDULER_TICK;
	scheduler->job    = scheduler_main;

	DEBUG(zlog_debug(scheduler->tag, "Initialize targets"));

    FILE *conf = fopen("./target.conf", "r");
    if(!conf) {
		zlog_error(scheduler->tag, "Fail to open conf");
		return -1;
	}
    enum {
        NONE,
        PLUG,
        NAME,
        VAL
    } status = NONE;

	char line[99];

    char type[98];
    char dlname[98], smname[98];
    int argc = 0;
    char argv[10][98];

    while(fgets(line, 99, conf)) {
        char first, remain[98];
        sscanf(line, "%c%s", &first, remain);
        if(first == '\n' || first == '#') {
        } else if(first == '<') {
            if(status != NONE) {
                zlog_error(scheduler->tag, "Unexpected character '<'");
                exit(1);
            }
            argc = 0;
            snprintf(type, 98, "%s", remain);
            snprintf(dlname, 98, "lib%s.so", type);
            snprintf(smname, 98, "%s_target_init", type);
            status = PLUG;
        } else if(first == '!') {
            if(status != PLUG) {
                zlog_error(scheduler->tag, "Unexpected character '!'");
                exit(1);
            }
            status = NAME;
        } else if(first == '-') {
            if(status == PLUG) {
                if(strncmp(remain, "OFF", 3) == 0) {
                    void *err;
                    while((err = fgets(line, 99, conf)) && line[0] != '>');
                    if(!err) {
                        zlog_error(scheduler->tag, "Expecting ']' for %s", type);
                        exit(1);
                    }
                    status = NONE;
                } else if(strncmp(remain, "ON", 2) != 0) {
                    zlog_error(scheduler->tag, "Status for %s is unknown: %s", type, remain);
                    exit(1);
                }
            } else if(status == NAME) {
                snprintf(argv[argc++], 98, "%s", remain);
                status = VAL;
            } else if(status == VAL) {
                snprintf(argv[argc++], 98, "%s", remain);
            } else {
                zlog_error(scheduler->tag, "Unexpected character '-'");
                exit(1);
            }
        } else if(first == '>') {
            if(status != VAL && status != PLUG) {
                zlog_error(scheduler->tag, "Unexpected character '>'");
                exit(1);
            }
            status = NONE;
            void *dl = dlopen(dlname, RTLD_LAZY);
            void *(*dynamic_target_init)(int, char **) = dlsym(dl, smname);
            void *target = 0;
            if(dlerror() || !(target = dynamic_target_init(argc, (char **)argv))) {
                target_fini(target);
                return -1;
            }
            if(target_init(target) < 0) continue;
            if(start(target) == 0)
                scheduler->targets[scheduler->targetc++] = target;
        } else {
            zlog_error(scheduler->tag, "Unexpected character");
            exit(1);
        }
    }
    fclose(conf);

	return 0;
}

void scheduler_fini(scheduler_t *scheduler) {
	runnable_fini((runnable_t *)scheduler);
}

void scheduler_main(void *_scheduler) {
	scheduler_t *scheduler = (scheduler_t *)_scheduler;

	for(int i=0; i<MAX_PLUGIN; ++i) {
		target_t *target = scheduler->targets[i];
		if(!target) continue;
		DEBUG(zlog_debug(scheduler->tag, "Plugin_%d [%c%c]", \
				i, \
				alive(target)   ?'A':'.', \
				outdated(target)?'O':'.'));

		if(alive(target) && outdated(target)) {
			DEBUG(zlog_debug(scheduler->tag, "Ping"));
			if(ping(target) < 0) {
				zlog_error(scheduler->tag, "Cannot ping the agent");
				restart(target);
			}
		}
	}
}

int _indexing(target_set_t *target_set) {
	if(!~target_set->up)
		return -1;
	int c;
	for(c=0; (target_set->up >> c) & 0x1; c = (c+1)%(8*sizeof(target_set_t)));
	target_set->up |= 0x1 << c ;
	return c;
}

void _deindexing(target_set_t *target_set, int c) {
	target_set->up &= 0 - (1<<c) - 1;
}
