#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h> // For strerror()
#include <unistd.h> // For getpid()
#include <sys/types.h>

#include <zlog.h>

#include "routine.h"
#include "metadata.h"
#include "sparser.h"
#include "scheduler.h"
#include "storage.h"
#include "plugin.h"
#include "util.h"
#include "daemon.h"

#define MAX_PLUGINS 5

int      pluginc;
plugin_t *plugins[MAX_PLUGINS] = {0};

int main(int argc, char **argv) {
    if(daemonize() < 0){
        fprintf(stderr, "Fail to make daemon process");
        exit(1);
    }

    if(zlog_init("/etc/maxgaugeair/.zlog.conf")) {
        fprintf(stderr, "zlog initiation failed\n");
        exit(1);
    }

    zlog_category_t *tag = zlog_get_category("main");

    /* Metadata */
    if(metadata_init() < 0) {
        DEBUG(zlog_error(tag, "Fail to initialize metadata"));
        exit(1);
    }

    /* Plugins */
    if(!(pluginc = sparse("/etc/maxgaugeair/plugin.conf", plugins))) {
        DEBUG(zlog_error(tag, "No plugin found"));
        exit(1);
    }

    /* Scheduler & Sender */
    routine_t sch, st;

    scheduler_init(&sch);
    storage_init(&st);

    routine_start(&sch);
    routine_start(&st);

    /* Start plugins */
    for(int i=0; i<pluginc; i++) {
        void *p = plugins[i];
        if(p) routine_start(p);
    }

    while(routine_alive(&sch) && routine_alive(&st)) {
        if(routine_overdue(&sch))
            if(routine_ping(&sch) < 0)
                routine_restart(&sch);
        if(routine_overdue(&st))
            routine_ping(&st);
        snyo_sleep(0.07);
    }

    /* Finalize */
    scheduler_fini(&sch);
    storage_fini(&st);

    zlog_fini();

    return 0;
}
