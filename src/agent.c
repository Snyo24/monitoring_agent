#include "scheduler.h"
#include "storage.h"
#include "shash.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>

#include <zlog.h>

int main(int argc, char **argv) {
    if(zlog_init("./zlog.conf")) {
        printf("zlog initiation failed\n");
        exit(1);
    }

    scheduler_t scheduler;
    storage_t storage;
    scheduler_init(&scheduler);
    storage_init(&storage);

    start_runnable(&scheduler);
    start_runnable(&storage);

    pthread_join(scheduler.running_thread, NULL);

    zlog_fini();
	return 0;
}
