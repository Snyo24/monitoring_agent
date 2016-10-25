#include "metadata.h"

#include <stdio.h>
#include <unistd.h>

#include "util.h"

char os[50];
char license[100];
char uuid[37];
int pid = -1;

static int get_os();
static int get_license();
static int get_uuid();
static int get_pid();

int metadata_init() {
	return get_os()
		| get_license()
		| get_uuid()
		| get_pid();
}

int get_os() {
	FILE *os_pipe;
	if(!(os_pipe = popen("uname -mrs", "r"))
	|| fscanf(os_pipe, "%[^\n]\n", os) != 1)
		return -1;
	return 0;
}

int get_license() {
	FILE *license_fd;
	if(!file_exist("res/license")
	|| !(license_fd = fopen("res/license", "r"))
	|| fscanf(license_fd, "%s", license) != 1) {
		printf("Cannot load license file\n");
		return -1;
	}
	return 0;
}

int get_uuid() {
	if(snprintf(uuid, 37, "%s", "550e8400-e29b-41d4-a716-446655440000") != 36)
		return -1;
	return 0;
}

int get_pid() {
	pid = getpid();
	FILE *pid_fd;
	if(!(pid_fd = fopen("res/pid", "w+")))
		return -1;
	fprintf(pid_fd, "%d\n", pid);
	return 0;
}
