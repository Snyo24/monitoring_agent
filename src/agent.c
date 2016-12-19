#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <zlog.h>

#include "runnable.h"
#include "metadata.h"
#include "sparser.h"
#include "scheduler.h"
#include "sender.h"
#include "plugin.h"
#include "util.h"

#define MAX_PLUGINS 5

scheduler_t sched;
sender_t    sndr;

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
    int n;
    void *plugins[MAX_PLUGINS] = {0};
    if(!(n = sparse("plugin.conf", plugins))) {
        DEBUG(zlog_error(tag, "No plugin found"));
        exit(1);
    }

    /* Scheduler & Sender */
    scheduler_init(&sched, n, plugins);
    sender_init(&sndr, n, plugins);

    runnable_start((runnable_t *)&sched);
    runnable_start((runnable_t *)&sndr);

    /* Start plugins */
    for(int i=0; i<MAX_PLUGINS; i++) {
        void *p = plugins[i];
        if(p) {
            runnable_start(p);
        }
    }

    while(runnable_alive((runnable_t *)&sched) && runnable_alive((runnable_t *)&sndr)) {
        if(runnable_overdue((runnable_t *)&sched))
            if(runnable_ping((runnable_t *)&sched) < 0)
                runnable_restart((runnable_t *)&sched);
        if(runnable_overdue((runnable_t *)&sndr))
            if(runnable_ping((runnable_t *)&sndr) < 0)
                runnable_restart((runnable_t *)&sndr);
        snyo_sleep(0.07);
    }

    /* Finalize */
    scheduler_fini(&sched);
    sender_fini(&sndr);

    zlog_fini();
    
    return 0;
}
