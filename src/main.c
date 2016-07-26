#include "aggregator.h"

#include <zlog.h>

int main(int argc, char **argv) {
    if (zlog_init("zlog.conf")) {
        printf("zlog initiation failed\n");
        return -1;
    }

	scheduler();

    zlog_fini();
	return 0;
}
