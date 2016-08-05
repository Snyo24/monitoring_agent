#include "scheduler.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <zlog.h>

#define DAEMON_PATH "./daemon"

int main(int argc, char **argv) {
	// zlog initiation
    if(zlog_init("./zlog.conf")) {
        printf("zlog initiation failed\n");
        exit(EXIT_FAILURE);
    }
	zlog_category_t *main_log = zlog_get_category("main");
	FILE *daemon = 0;
	char cmd[100];
	pid_t pid, sid;
	struct stat st;
	// Run a daemon
	if(argc > 1) {
		if(argv[1][0] == '0') {
			daemon = fopen(DAEMON_PATH, "r+");
			if(!daemon) {
				zlog_error(main_log, "Agent daemon is not running");
				exit(EXIT_FAILURE);
			}
			if(fscanf(daemon, "%d", &pid) != 1) {
				zlog_error(main_log, "Failed to get daemon pid");
				exit(EXIT_FAILURE);
			}
			sprintf(cmd, "kill %d", pid);
			printf("KILL DAEMON\n");
			if(system(cmd) < 0) {
				zlog_fatal(main_log, "Failed to kill daemon");
				exit(EXIT_FAILURE);
			}
			sprintf(cmd, "rm ./daemon");
			if(system(cmd) < 0) {
				zlog_fatal(main_log, "Failed to delete daemon file");
				exit(EXIT_FAILURE);
			}
			fclose(daemon);
			exit(EXIT_SUCCESS);
		} else if(argv[1][0] == '1') {
			if(!stat(DAEMON_PATH, &st)) {
				zlog_error(main_log, "Daemon is already running");
				exit(EXIT_FAILURE);
			}
			pid = fork();
			if(pid < 0) 
				exit(EXIT_FAILURE);
			if(pid > 0) { // parent
				daemon = fopen(DAEMON_PATH, "w+");
				fprintf(daemon, "%d", pid);
				printf("START DAEMON\n");
				fclose(daemon);

				exit(EXIT_SUCCESS);
			}

			umask(0);

			sid = setsid();
			if(sid < 0) 
				exit(EXIT_FAILURE);

			// TODO causes yaml file path error
			// if((chdir("/")) < 0) 
			// 	exit(EXIT_FAILURE);

			// Close standard IO
			close(0);
			close(1);
			close(2);
		}
	}

	schedule();

    zlog_fini();
	return 0;
}
