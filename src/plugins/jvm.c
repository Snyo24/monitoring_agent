/**
 * @file jvm.c
 * @author Snyo
 */
#include "plugins/jvm.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

#include "plugin.h"
#include "metadata.h"
#include "sender.h"

#define JVM_PLUGIN_TICK 1
#define JVM_PLUGIN_CAPACITY 1
#define MAX_CONNECTION 3

typedef struct _jvm_spec {
	int jvm_sock;
	int client_num;
	int client_sock[MAX_CONNECTION];
	int epoll_fd;
} jvm_spec_t;

static int jvm_spec_init(jvm_spec_t *spec, unsigned short port);

int jvm_plugin_init(jvm_plugin_t *plugin, char *option) {
    unsigned short port;
    if(sscanf(option, "%hu", &port) != 1) return -1;

	plugin->spec = malloc(sizeof(jvm_spec_t));
	if(!plugin->spec) return -1;
	if(jvm_spec_init(plugin->spec, port) < 0) {
		free(plugin->spec);
		return -1;
	}

	plugin->type     = "jvm_linux_1.0";
	plugin->ip       = 0;
	plugin->period   = JVM_PLUGIN_TICK;
	plugin->capacity = JVM_PLUGIN_CAPACITY;

	plugin->collect  = collect_jvm_metrics;
	plugin->fini     = jvm_plugin_fini;

	return 0;
}

int jvm_spec_init(jvm_spec_t *spec, unsigned short port) {
	if((spec->jvm_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;

	int t = 1;
	setsockopt(spec->jvm_sock, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t));

	struct sockaddr_in jvm_addr;
	memset(&jvm_addr, 0, sizeof(jvm_addr));
	jvm_addr.sin_family      = AF_INET;
	jvm_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	jvm_addr.sin_port        = htons(port);

	if(bind(spec->jvm_sock, (struct sockaddr *)&jvm_addr, sizeof(jvm_addr)) < 0
			|| listen(spec->jvm_sock, MAX_CONNECTION) < 0) {
		close(spec->jvm_sock);
		return -1;
	}

	struct epoll_event init_event;
	init_event.events = EPOLLIN;
	init_event.data.fd = spec->jvm_sock;
	if((spec->epoll_fd = epoll_create(MAX_CONNECTION * 2)) < 0
			|| epoll_ctl(spec->epoll_fd, EPOLL_CTL_ADD, spec->jvm_sock, &init_event) < 0) {
		close(spec->jvm_sock);
		return -1;
	}

	return 0;
}

void jvm_plugin_fini(jvm_plugin_t *plugin) {
	if(!plugin)
		return;

	if(plugin->spec) {
		free(plugin->spec);
	}
}

void collect_jvm_metrics(jvm_plugin_t *plugin) {
	jvm_spec_t *spec = (jvm_spec_t *)plugin->spec;
	struct epoll_event events[MAX_CONNECTION];

	int event_count = epoll_wait(spec->epoll_fd, events, MAX_CONNECTION, 0);
	char oob[1000];
	if(event_count < 0)
		return;
	for(int i=0; i<event_count; ++i) {
		int fd = events[i].data.fd;
		int ev = events[i].events;
		if(fd == spec->jvm_sock) {
			struct sockaddr_in client_addr;
			unsigned int client_len = sizeof(client_addr);
			int client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
			struct epoll_event init_event;
			init_event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR; 
			init_event.data.fd = client_fd;
			epoll_ctl(spec->epoll_fd, EPOLL_CTL_ADD, client_fd, &init_event);
			snprintf(oob, 1000, "{\"license\":\"%s\",\"target_num\":%d,\"uuid\":\"%s\",\"status\":true}", license, plugin->index, uuid);
			alert_post(oob);
		}
		else {
			char buf[1024];
			if(ev & (EPOLLRDHUP | EPOLLERR)) {
				close(fd);
				epoll_ctl(spec->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
				snprintf(oob, 1000, "{\"license\":\"%s\",\"target_num\":%d,\"uuid\":\"%s\",\"status\":false}", license, plugin->index, uuid);
				alert_post(oob);
				break;
			} else if(ev & EPOLLIN) {
				if(read(fd, buf, 1024) <= 0) break;
				json_object *obj = json_tokener_parse(buf);
				if(!obj) break;
				json_object_put(plugin->metric);
				json_object_put(plugin->values);
				plugin->metric = json_object_object_get(obj, "metrics");
				plugin->values = json_object_object_get(obj, "values");
				plugin->holding++;
			}
		}
	}
}
