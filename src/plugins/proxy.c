/**
 * @file proxy.c
 * @author Snyo
 */
#include "plugins/proxy.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include "pluggable.h"

#define PROXY_PLUGIN_TICK MS_PER_S*1
#define PROXY_PLUGIN_FULL 1
#define MAX_CONNECTION 3

typedef struct _proxy_spec {
	int proxy_sock;
	int client_num;
	int client_sock[MAX_CONNECTION];
	int epoll_fd;
} proxy_spec_t;

static int proxy_spec_init(proxy_spec_t *spec);

int proxy_plugin_init(proxy_plugin_t *plugin) {
	plugin->spec = malloc(sizeof(proxy_spec_t));
	if(!plugin->spec) return -1;
	if(proxy_spec_init(plugin->spec) < 0) {
		free(plugin->spec);
		return -1;
	}

	plugin->target_type = "jvm_linux_1.0";
	plugin->target_ip  = 0;
	plugin->period     = PROXY_PLUGIN_TICK;
	plugin->full_count = PROXY_PLUGIN_FULL;

	plugin->collect    = collect_proxy_metrics;
	plugin->fini       = proxy_plugin_fini;

	return 0;
}

int proxy_spec_init(proxy_spec_t *spec) {
	if((spec->proxy_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;

	int t = 1;
	setsockopt(spec->proxy_sock, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t));

	struct sockaddr_in proxy_addr;
	memset(&proxy_addr, 0, sizeof(proxy_addr));
	proxy_addr.sin_family      = AF_INET;
	proxy_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	proxy_addr.sin_port        = htons(8081);

	if(bind(spec->proxy_sock, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr)) < 0
	|| listen(spec->proxy_sock, MAX_CONNECTION) < 0) {
		close(spec->proxy_sock);
		return -1;
	}
	
	struct epoll_event init_event;
	init_event.events = EPOLLIN;
	init_event.data.fd = spec->proxy_sock;
	if((spec->epoll_fd = epoll_create(MAX_CONNECTION * 2)) < 0
	|| epoll_ctl(spec->epoll_fd, EPOLL_CTL_ADD, spec->proxy_sock, &init_event) < 0) {
		close(spec->proxy_sock);
		return -1;
	}

	return 0;
}

void proxy_plugin_fini(proxy_plugin_t *plugin) {
	if(!plugin)
		return;

	if(plugin->spec) {
		free(plugin->spec);
	}
}

void collect_proxy_metrics(proxy_plugin_t *plugin) {
	proxy_spec_t *spec = (proxy_spec_t *)plugin->spec;
	struct epoll_event events[MAX_CONNECTION];

	int event_count = epoll_wait(spec->epoll_fd, events, MAX_CONNECTION, 0);
	if(event_count < 0)
		return;
	for(int i=0; i<event_count; ++i) {
		int fd = events[i].data.fd;
		int ev = events[i].events;
		if(fd == spec->proxy_sock) {
			struct sockaddr_in client_addr;
			unsigned int client_len = sizeof(client_addr);
			int client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
			struct epoll_event init_event;
			init_event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR; 
			init_event.data.fd = client_fd;
			epoll_ctl(spec->epoll_fd, EPOLL_CTL_ADD, client_fd, &init_event);
		}
		else {
			char buf[1024];
			if(ev & (EPOLLRDHUP | EPOLLERR)) {
				close(fd);
				epoll_ctl(spec->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
				break;
			} else if(ev & EPOLLIN) {
				if(read(fd, buf, 1024) <= 0) break;
				json_object *obj = json_tokener_parse(buf);
				if(!obj) break;
				json_object_put(plugin->metric_names);
				json_object_put(plugin->values);
				plugin->metric_names = json_object_object_get(obj, "metrics");
				plugin->values = json_object_object_get(obj, "values");
				plugin->holding++;
			}
		}
	}
}
