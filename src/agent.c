#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <zlog.h>

#include "scheduler.h"
#include "storage.h"
#include "sender.h"
#include "shash.h"
#include "plugins/os.h"
#include "pluggable.h"

scheduler_t scheduler;
storage_t   storage;
sender_t    sender;

char license[] = "license_exem4";
char uuid[] = "550e8400-e29b-41d4-a716-446655440000";
char os[100];

int main(int argc, char **argv) {
    if(zlog_init("./zlog.conf")) {
        printf("zlog initiation failed\n");
        exit(1);
    }

    /* Initiation */
    scheduler_init(&scheduler);
    storage_init(&storage);
    sender_init(&sender);
    sender_set_reg_uri(&sender);
    FILE *os_version = popen("uname -mrs", "r");
    if(fscanf(os_version, "%[^\n]\n", os) != 1) exit(1);
	printf("%s\n", os);
    if(sender_post(&sender, \
    "{\
\"license\":\"license_exem4\",\
\"uuid\":\"550e8400-e29b-41d4-a716-446655440000\",\
\"agent_type\":\"linux_1.0\",\
\"target_type\":[\"linux_linux_1.0\",\"linux_linux_1.0\"],\
\"target_num\":[1, 2]\
}") < 0) exit(1);
    sender_set_met_uri(&sender);

    /* Plugins */
    plugin_t *p = new_plugin("os");
    start(p);
    shash_insert(scheduler.spec, "os", p);

	/* Run */
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
