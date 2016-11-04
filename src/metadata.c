#include "metadata.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <openssl/md5.h>

#include "util.h"

char hostname[100];
char os[50];
char license[100];
char uuid[33];
char *agent_ip;
char *agent_type;

static int get_hostname();
static int get_os();
static int get_license();
static int get_uuid();
static int get_agent_ip();
static int get_agent_type();
static int get_pid();
static int get_mac_addr();

int metadata_init() {
	return get_hostname()!= -1
	    && get_os()      != -1
		&& get_license() != -1
		&& get_uuid()    != -1
	    && get_agent_ip()!= -1
		&& get_agent_type()!=-1
		&& get_pid()     != -1;
}

int get_hostname() {
	return gethostname(hostname, 100);
}

int get_os() {
	FILE *os_pipe = popen("uname -mrs", "r");
	if(!os_pipe) return -1;
	int success = fscanf(os_pipe, "%[^\n]\n", os) == 1;
	pclose(os_pipe);
	if(!success) return -1;
	return 0;
}

int get_agent_ip() {
	struct addrinfo hints;
	struct addrinfo *result;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if(getaddrinfo(hostname, NULL, &hints, &result) != 0)
		return -1;
	agent_ip = inet_ntoa(((struct sockaddr_in *)result->ai_addr)->sin_addr);
	freeaddrinfo(result);
	return 0;
}

int get_agent_type() {
	agent_type = "linux_1.0";
	return 0;
}

int get_license() {
	FILE *license_fd = fopen("res/license", "r");
	if(!license_fd) return -1;
	int success = fscanf(license_fd, "%s", license) == 1;
	fclose(license_fd);
	if(!success) return -1;
	return 0;
}

int get_uuid() {
	FILE *uuid_fd = fopen("res/uuid", "r");
	if(uuid_fd) {
		int success = fscanf(uuid_fd, "%s", uuid) == 1;
		fclose(uuid_fd);
		if(success) return 0;
	}
	char md_str[100];
	if(get_mac_addr(md_str) < 0) return -1;
	int n = 12;
	n += snprintf(md_str, 100-n, "%lu", get_timestamp());

	unsigned char md_hash[MD5_DIGEST_LENGTH];
	MD5((unsigned char *)&md_str, strlen(md_str), (unsigned char *)&md_hash);
	for(int i=0; i<16; ++i)
		sprintf(uuid+i*2, "%02x", (unsigned int)md_hash[i]);

	uuid_fd = fopen("res/uuid", "w+");
	if(!uuid_fd) return -1;
	fprintf(uuid_fd, "%s\n", uuid);
	fclose(uuid_fd);
	return 0;
}

int get_pid() {
	FILE *pid_fd;
	if(!(pid_fd = fopen("res/pid", "w+")))
		return -1;
	fprintf(pid_fd, "%d\n", getpid());
	fclose(pid_fd);
	return 0;
}

int get_mac_addr(char *mac) {
    FILE *fd = popen("ls /sys/class/net", "r");
    if(fd < 0) return -1;
    char net_name[100];
    int loaded = 0;
    while(fscanf(fd, "%100s", net_name) == 1)
       if(strncmp(net_name, "lo", 2) != 0) {
            loaded = 1;
            break;
        }
    pclose(fd);
    if(!loaded) return -1;
    char addr[100];
    snprintf(addr, 100, "/sys/class/net/%s/address", net_name);
    if((fd = fopen(addr, "r")) < 0)
        return -1;
    loaded = fscanf(fd, "%2s:%2s:%2s:%2s:%2s:%2s", &mac[0], &mac[2], &mac[4],\
	                                               &mac[6], &mac[8], &mac[10]) == 6;
    fclose(fd);
    if(!loaded) return -1;
    return 0;
}
