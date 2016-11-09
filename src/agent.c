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
    if(zlog_init("conf/zlog.conf")) {
        printf("zlog initiation failed\n");
        exit(1);
    }

    /* Initiation */
    scheduler_init(&scheduler);
    storage_init(&storage);
    sender_init(&sender);

	/* Metadata register */
	if(metadata_init() < 0)
		exit(1);
    sender_set_reg_uri(&sender);
	char reg_str[1000];
	int n = 0;
	n += snprintf(reg_str, 1000, "{\
\"os\":\"%s\",\
\"hostname\":\"%s\",\
\"license\":\"%s\",\
\"uuid\":\"%s\",\
\"agent_ip\":\"%s\",\
\"agent_type\":\"%s\",\
\"target_type\":[", os, hostname, license, uuid, agent_ip, agent_type);
	int sw = 0;
	for(int i=0; i<10; ++i) {
		if(scheduler.plugins[i])
			n += snprintf(reg_str+n, 1000-n, "%s\"%s\"", sw++?",":"", scheduler.plugins[i]->target_type);
	}
	n += snprintf(reg_str+n, 1000-n, "],\"target_num\":[");
	sw = 0;
	for(int i=0; i<10; ++i) {
		if(scheduler.plugins[i])
			n += snprintf(reg_str+n, 1000-n, "%s%d", sw++?",":"", scheduler.plugins[i]->index);
	}
	n += snprintf(reg_str+n, 1000-n, "]}");
	if(sender_post(&sender, reg_str) < 0) exit(1);

	/* Run */
    sender_set_met_uri(&sender);
    start_runnable((runnable_t *)&scheduler);
    start_runnable((runnable_t *)&storage);
    start_runnable((runnable_t *)&sender);
	start_plugins(&scheduler);

    /* Finalize */
    scheduler_fini(&scheduler);
    storage_fini(&storage);
    sender_fini(&sender);

    zlog_fini();
    return 0;
}
