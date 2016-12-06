#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <zlog.h>

#include "metadata.h"
#include "scheduler.h"
#include "storage.h"
#include "sender.h"
#include "plugin.h"

scheduler_t scheduler;
storage_t   storage;
sender_t    sender;

int main(int argc, char **argv) {
    if(zlog_init(".zlog.conf")) {
        printf("zlog initiation failed\n");
        exit(1);
    }

    /* Initiation */
    scheduler_init(&scheduler);
    storage_init(&storage);
    sender_init(&sender);

    /* Metadata register */
    if(metadata_init() < 0) {
        printf("Fail to initialize metadata\n");
        exit(1);
    }
    sender_set_reg_uri(&sender);
    char reg_str[1000];
    int n = 0;
    n += snprintf(reg_str, 1000, "{\n\
            \"os\":\"%s\",\n\
            \"hostname\":\"%s\",\n\
            \"license\":\"%s\",\n\
            \"uuid\":\"%s\",\n\
            \"agent_ip\":\"%s\",\n\
            \"agent_type\":\"%s\",\n\
            \"target_type\":[",\
            os, host, license, uuid, ip, type);
    int sw = 0;
    for(int i=0; i<10; ++i) {
        if(scheduler.plugins[i])
            n += snprintf(reg_str+n, 1000-n, "%s\"%s\"", sw++?",":"", ((plugin_t *)scheduler.plugins[i])->type);
    }
    if(sw == 0) {
        printf("No plugins initialized\n");
        exit(1);
    }
    n += snprintf(reg_str+n, 1000-n, "],\n\
            \"target_num\":[");
    sw = 0;
    for(int i=0; i<10; ++i) {
        if(scheduler.plugins[i])
            n += snprintf(reg_str+n, 1000-n, "%s%d", sw++?",":"", ((plugin_t *)scheduler.plugins[i])->index);
    }
    n += snprintf(reg_str+n, 1000-n, "]\n}");
    if(sender_post(&sender, reg_str) < 0);// exit(1);

    /* Run */
    sender_set_met_uri(&sender);
    start_runnable((runnable_t *)&scheduler);
    start_runnable((runnable_t *)&storage);
    start_runnable((runnable_t *)&sender);

    /* Finalize */
    scheduler_fini(&scheduler);
    storage_fini(&storage);
    sender_fini(&sender);

    zlog_fini();
    return 0;
}
