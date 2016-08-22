#include "scheduler.h"

#include <stdio.h>
#include <stdlib.h>
#include <zlog.h>

int main(int argc, char **argv) {
    if(zlog_init("./zlog.conf")) {
        printf("zlog initiation failed\n");
        exit(EXIT_FAILURE);
    }

	schedule();

    zlog_fini();
	return 0;
}
