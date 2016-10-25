#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <zlog.h>

#include "metadata.h"
#include "scheduler.h"
#include "storage.h"
#include "sender.h"
#include "shash.h"
#include "pluggable.h"

scheduler_t scheduler;
storage_t   storage;
sender_t    sender;

int main(int argc, char **argv) {
    if(zlog_init("res/zlog.conf")) {
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
	snprintf(reg_str, 1000, "{\
\"license\":\"%s\",\
\"uuid\":\"%s\",\
\"agent_type\":\"linux_1.0\",\
\"target_type\":[\"linux_linux_1.0\",\"linux_linux_1.0\"],\
\"target_num\":[0, 1]\
}", license, uuid);
    if(sender_post(&sender, reg_str) < 0);// exit(1);

	/* Run */
    sender_set_met_uri(&sender);
    start_runnable(&scheduler);
    start_runnable(&storage);
    start_runnable(&sender);

    /* Done */
    pthread_join(scheduler.running_thread, NULL);
    pthread_join(storage.running_thread, NULL);
    pthread_join(sender.running_thread, NULL);

    /* Finalize */
    scheduler_fini(&scheduler);
    storage_fini(&storage);
    sender_fini(&sender);

    zlog_fini();
    return 0;
}
