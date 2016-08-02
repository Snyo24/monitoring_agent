#include "aggregator.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <zlog.h>

int main(int argc, char **argv) {
	FILE *pfd = 0;
	char cmd[100];
	pid_t pid, sid;
	struct stat st;
	if(argc > 1) {
		switch(argv[1][0]) {
			case '0':
			pfd = fopen("./daemon", "r+");
			fscanf(pfd, "%d", &pid);
			sprintf(cmd, "kill %d", pid);
			printf("KILL DAEMON\n");
			system(cmd);
			sprintf(cmd, "rm ./daemon");
			system(cmd);
			exit(EXIT_SUCCESS);

			case '1':
			if(!stat("./daemon", &st)) {
				printf("Daemon is already running\n");
				exit(EXIT_FAILURE);
			}
			pfd = fopen("./daemon", "w+");
			pid = fork();
			if(pid < 0) 
				exit(EXIT_FAILURE);
			if(pid > 0) {
				fprintf(pfd, "%d", pid);
				printf("START DAEMON\n");
				exit(EXIT_SUCCESS);
			}

			umask(0);

			sid = setsid();
			if(sid < 0) 
				exit(EXIT_FAILURE);

			if((chdir("/")) < 0) 
				exit(EXIT_FAILURE);

			close(0);
			close(1);
			close(2);
		}
		fclose(pfd);
	}
	else {
	    if (zlog_init("zlog.conf")) {
	        printf("zlog initiation failed\n");
	        return -1;
	    }
	}

	scheduler();

    zlog_fini();
	return 0;
}
