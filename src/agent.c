#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <zlog.h>

#include "runnable.h"
#include "metadata.h"
#include "sparser.h"
#include "scheduler.h"
#include "storage.h"
#include "plugin.h"
#include "util.h"

#define MAX_PLUGINS 5

int      pluginc;
plugin_t *plugins[MAX_PLUGINS] = {0};

int main(int argc, char **argv) {
    if(zlog_init(".zlog.conf")) {
        printf("zlog initiation failed\n");
        exit(1);
    }

    zlog_category_t *tag = zlog_get_category("main");

    /* Metadata */
    if(metadata_init() < 0) {
        DEBUG(zlog_error(tag, "Fail to initialize metadata"));
        exit(1);
    }

    /* Plugins */
    if(!(pluginc = sparse("plugin.conf", plugins))) {
        DEBUG(zlog_error(tag, "No plugin found"));
        exit(1);
    }

    /* Scheduler & Sender */
    runnable_t sch, st;

    scheduler_init(&sch);
    storage_init(&st);

    runnable_start(&sch);
    runnable_start(&st);

    /* Start plugins */
    for(int i=0; i<pluginc; i++) {
        void *p = plugins[i];
        if(p) runnable_start(p);
    }

    while(runnable_alive(&sch) && runnable_alive(&st)) {
        if(runnable_overdue(&sch))
            if(runnable_ping(&sch) < 0)
                runnable_restart(&sch);
        if(runnable_overdue(&st))
            runnable_ping(&st);
        snyo_sleep(0.07);
    }

    /* Finalize */
    scheduler_fini(&sch);
    storage_fini(&st);

    zlog_fini();
    
    return 0;
}
