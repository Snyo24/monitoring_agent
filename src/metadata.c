#include "metadata.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "util.h"

char os[50];
char aip[17];
char host[99];
char type[20];
unsigned long long aid;
char license[99];

static int get_pid();

int get_os() {
	FILE *os_pipe = popen("awk -F'=' '$1~\"PRETTY_NAME\"{gsub(\"\\\"\",\"\");print$2}' /etc/os-release", "r");
	if(!os_pipe) return -1;
	int success = fscanf(os_pipe, "%50[^\n]\n", os)==1;
	pclose(os_pipe);
	if(!success) return -1;
	return 0;
}

int get_host() {
	return gethostname(host, 99);
}

int get_ip() {
	struct addrinfo hints;
	struct addrinfo *result;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if(getaddrinfo(host, NULL, &hints, &result) != 0)
		return -1;
    char *addr = inet_ntoa(((struct sockaddr_in *)result->ai_addr)->sin_addr);
	snprintf(aip, 17, "%s", addr);
	freeaddrinfo(result);
	return 0;
}

int get_type() {
    snprintf(type, 20, "ubuntu_1.0");
	return 0;
}

int get_aid() {
	FILE *aid_fd = fopen(".aid", "r");
	if(aid_fd) {
		int success = fscanf(aid_fd, "%llu", &aid) == 1;
		fclose(aid_fd);
		if(success) return 0;
	}
	
    aid = epoch_time()*epoch_time()*epoch_time();
	aid_fd = fopen(".aid", "w+");
	if(!aid_fd) return -1;
	fprintf(aid_fd, "%llu\n", aid);
	fclose(aid_fd);
	return 0;
}

int get_license() {
	FILE *license_fd = fopen("cfg/license", "r");
	if(!license_fd) return -1;
    char tmp[1000];
	int success = fscanf(license_fd, "%[^:]:%*[:^\t^ ]%s", tmp, license) == 2;
	fclose(license_fd);
	if(!success) return -1;
	return 0;
}

int get_pid() {
	FILE *pid_fd;
	if(!(pid_fd = fopen(".pid", "w+")))
		return -1;
	fprintf(pid_fd, "%d\n", getpid());
	fclose(pid_fd);
	return 0;
}

int metadata_init() {
	return -(get_os()      < 0
          || get_host()    < 0
		  || get_ip()      < 0
		  || get_type()    < 0
		  || get_aid()    < 0
		  || get_license() < 0
		  || get_pid()     < 0);
}
