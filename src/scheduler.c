/** @file scheduler.c @author Snyo */

#include "scheduler.h"

#include "agent.h"
#include "agents/dummy.h"
#include "agents/mysql.h"
#include "agents/network.h"
#include "snyohash.h"
#include "sender.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <unistd.h>

#include <zlog.h>

#include <arpa/inet.h>
#include <netdb.h>

#define SCHEDULER_TICK NS_PER_S/1

#define REG_URI "http://10.10.202.115:8082/v1/agents"
#define METRIC_URI "http://10.10.202.115:8082/v1/metrics"

void *scheduler_tag;

const char *agent_names[] = {
	"NET"
};
const char *agent_types[] = {
	"net_linux_v1"
};
agent_t *(*agent_constructors[])(const char *, const char *) = {
	new_network_agent
};
shash_t *agents;

char g_license[100];
char g_uuid[40];

int get_license(char *license);
int get_uuid(char *uuid);
int get_agents_names(char *agents);
int get_agents_types(char *agents);
int get_agents_ids(char *agents);
int get_hostname(char *hostname);

// TODO exception handling
int scheduler_init() {
	/* Logging */
	scheduler_tag = (void *)zlog_get_category("Scheduler");
    if (!scheduler_tag) return -1;

    /* Agents */
	zlog_debug(scheduler_tag, "Initialize agents");
	agents = shash_init();
	if(!agents) {
		zlog_error(scheduler_tag, "Failed to create an agent hash");
    	return -1;
	}

	for(int i=0; i<NUMBER_OF(agent_names); ++i) {
		zlog_debug(scheduler_tag, "Create an agent \'%s\'", agent_names[i]);
		agent_t *agent = (agent_constructors[i])(agent_names[i], NULL);
		if(!agent) {
			zlog_error(scheduler_tag, "Failed to create \'%s\'", agent_names[i]);
		} else {
			zlog_debug(scheduler_tag, "Start the agent \'%s\'", agent_names[i]);
			if(!start(agent)) {
				shash_insert(agents, agent_names[i], agent);
			} else {
				delete_agent(agent);
			}
		}
	}

	/* Register topics */
	sender_set_uri(REG_URI);
	zlog_debug(scheduler_tag, "Register topic to the server");
	char reg_json[200], *ptr;
	ptr = reg_json;
	get_license(g_license);
	ptr += sprintf(ptr, "{\"license\":\"%s", g_license);
	get_uuid(g_uuid);
	ptr += sprintf(ptr, "\",\"uuid\":\"%.*s", 36, g_uuid);
	ptr += sprintf(ptr, "\",\"target_name\":[");
	ptr += get_agents_names(ptr);
	ptr += sprintf(ptr, "],\"target_type\":[");
	ptr += get_agents_types(ptr);
	ptr += sprintf(ptr, "],\"target_id\":[");
	ptr += get_agents_ids(ptr);
	ptr += sprintf(ptr, "],\"hostname\":\"");
	ptr += get_hostname(ptr);
	ptr += sprintf(ptr, "\"}");
	printf("%s\n", reg_json);
	if(sender_post(reg_json) < 0) {
		zlog_error(scheduler_tag, "Fail to register topic");
    	return -1;
	}
	sender_set_uri(METRIC_URI);

	return 0;
}

void schedule() {
	while(1) {
		for(int i=0; i<NUMBER_OF(agent_names); ++i) {
			agent_t *agent = (agent_t *)shash_search(agents, agent_names[i]);
			if(!agent) continue;
			zlog_debug(scheduler_tag, "Status of \'%s\' [%c%c%c]", \
			                                agent_names[i], \
			                                outdated(agent)?'O':'.', \
			                                busy(agent)?'B':'.', \
			                                busy(agent)&&timeup(agent)?'T':'.');
			if(busy(agent)) {
				if(timeup(agent)) {
					zlog_fatal(scheduler_tag, "Restarting \'%s\'", agent->name);
					restart(agent);
				}
			} else if(outdated(agent)) {
				zlog_debug(scheduler_tag, "Poking \'%s\'", agent->name);
				poke(agent);
			}
		}

		zlog_debug(scheduler_tag, "Sleep for %lums", SCHEDULER_TICK/1000000);
		snyo_sleep(SCHEDULER_TICK);
	}
}

void scheduler_fini() {
	// TODO
	sender_fini();
}


/**
 * These are used only in this source file
 */

int get_license(char *license) {
	return sprintf(license, "%s", "efgh");
}

int get_uuid(char *uuid) {
	sprintf(uuid, "550e8400-e29b-41d4-a716-446655440000");
	return 36;
}

int get_agents_names(char *agents) {
	int n = 0;
	for(int i=0; i<NUMBER_OF(agent_names); ++i) {
		n += sprintf(agents+n, "\"%s\"", agent_names[i]);
		if(i < NUMBER_OF(agent_names)-1)
			sprintf(agents+n++, ",");
	}
	return n;
}

int get_agents_types(char *agents) {
	int n = 0;
	for(int i=0; i<NUMBER_OF(agent_names); ++i) {
		n += sprintf(agents+n, "\"%s\"", agent_types[i]);
		if(i < NUMBER_OF(agent_names)-1)
			sprintf(agents+n++, ",");
	}
	return n;
}

int get_agents_ids(char *agents) {
	int n = 0;
	for(int i=0; i<NUMBER_OF(agent_names); ++i) {
		n += sprintf(agents+n, "\"test\"");
		if(i < NUMBER_OF(agent_names)-1)
			sprintf(agents+n++, ",");
	}
	return n;
}

int get_hostname(char *hostname) {
	if(!gethostname(hostname, 100)){
	struct hostent *ip = gethostbyname(hostname);
	printf("%s\n", inet_ntoa(*((struct in_addr *)ip->h_addr_list[0])));
		return strlen(hostname);
	}
	return sprintf(hostname, "fail_to_get_hostname");
}
