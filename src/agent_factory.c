#include "snyohash.h"
#include "scheduler.h"
#include "sender.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <zlog.h>

int main(int argc, char **argv) {
    if(zlog_init("./zlog.conf")) {
        printf("zlog initiation failed\n");
        exit(1);
    }

    char license[100];
    char uuid[37];

    shash_t *parsed = shash_init();
    if(parse_conf("./conf/license.conf", parsed, license, uuid) < 0) {
        printf("Fail to parse configuration file\n");
    }
    printf("%s %s\n", (char *)shash_search(parsed, "License"), (char *)shash_search(parsed, "UUID"));
    shash_fini(parsed);

    /* Sender init */
	if(sender_init() < 0 || scheduler_init() < 0) {
		printf("Fail to initialize\n");
		exit(1);
	}
    sender_start();
	schedule();
	scheduler_fini();

    zlog_fini();
	return 0;
}
