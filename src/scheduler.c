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

#include "plugin.h"
#include "util.h"

#define SCHEDULER_TICK 0.5

typedef struct plugin_set_t {
	unsigned up;
} plugin_set_t;

//static plugin_set_t plugin_set = {0};

//static int  _indexing();
//static void _deindexing();

int scheduler_init(scheduler_t *scheduler) {
	if(runnable_init((runnable_t *)&scheduler) < 0)
		return -1;

	scheduler->tag = zlog_get_category("scheduler");
	if(!scheduler->tag);

	DEBUG(zlog_debug(scheduler->tag, "Initailize a plugin table"));
	memset(scheduler->plugins, 0, sizeof(plugin_t *)*MAX_PLUGIN);
    scheduler->pluginc = 0;

	scheduler->period = SCHEDULER_TICK;
	scheduler->job    = scheduler_main;

	DEBUG(zlog_debug(scheduler->tag, "Initialize plugins"));

    FILE *conf = fopen("./plugin.conf", "r");
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
            snprintf(smname, 98, "%s_plugin_init", type);
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
            void *(*dynamic_plugin_init)(int, char **) = dlsym(dl, smname);
            void *plugin = 0;
            if(dlerror() || !(plugin = dynamic_plugin_init(argc, (char **)argv))) {
                plugin_fini(plugin);
                return -1;
            }
            if(plugin_init(plugin) < 0) continue;
            start(plugin);
            scheduler->plugins[scheduler->pluginc++] = plugin;
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
		plugin_t *plugin = scheduler->plugins[i];
		if(!plugin) continue;
		DEBUG(zlog_debug(scheduler->tag, "Plugin_%d [%c%c]", \
				i, \
				alive(plugin)   ?'A':'.', \
				outdated(plugin)?'O':'.'));

		if(alive(plugin) && outdated(plugin)) {
			DEBUG(zlog_debug(scheduler->tag, "Poking"));
			if(poke(plugin) < 0) {
				zlog_error(scheduler->tag, "Cannot poke the agent");
				restart(plugin);
			}
		}
	}
}

int _indexing(plugin_set_t *plugin_set) {
	if(!~plugin_set->up)
		return -1;
	int c;
	for(c=0; (plugin_set->up >> c) & 0x1; c = (c+1)%(8*sizeof(plugin_set_t)));
	plugin_set->up |= 0x1 << c ;
	return c;
}

void _deindexing(plugin_set_t *plugin_set, int c) {
	plugin_set->up &= 0 - (1<<c) - 1;
}
