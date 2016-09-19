#include "scheduler.h"
#include "sender.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <zlog.h>

void get_configuration(const char *conf, char *license, char *uuid) {
    if(!file_exist((char *)conf)) {
    	sprintf(license, "%s", "Bg1F1T4WOI0BW3MUSi4PZHSyeCwn12H1");
    	sprintf(uuid, "%s", "550e8400-e29b-41d4-a716-446655440000");
    	FILE *fp = fopen(conf, "w");
    	fprintf(fp, "%s\n%s\n", license, uuid);
    	fclose(fp);
    } else {
    	FILE *fp = fopen(conf, "r");
    	if(fscanf(fp, "%s%s", license, uuid) != 2) {
    		fclose(fp);
    		exit(1);
    	}
    }
}

int main(int argc, char **argv) {
    if(zlog_init("./zlog.conf")) {
        printf("zlog initiation failed\n");
        exit(1);
    }

    char license[100];
    char uuid[37];
    get_configuration("user.conf", license, uuid);

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
