/** @file scheduler.c @author Snyo */

#include "scheduler.h"

#include "agent.h"
#include "agents/mysql.h"
#include "snyohash.h"
#include "sender.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <uuid/uuid.h>

void *scheduler_tag;

int agent_number;
char *agent_names[] = {
	"MySQL1",
	"MySQL2",
	"MySQL3"
};
shash_t *agents;

sender_t *global_sender;

char g_license[100];
char g_uuid[40];

int get_license(char *license);
int get_hostname(char *hostname);
int get_uuid(char *uuid);
int get_agents(char *agents);

// TODO exception handling
void initialize() {
	/* Logging */
	scheduler_tag = (void *)zlog_get_category("scheduler");
    if (!scheduler_tag) return;

    /* Sender */
	zlog_info(scheduler_tag, "Initialize sender");
	global_sender = new_sender("http://192.168.122.37:8082/v1/agents");
    if (!global_sender) {
    	zlog_error(scheduler_tag, "Cannot create a new sender");
    	return;
    }
	start_sender(global_sender);

	agent_number = sizeof(agent_names)/sizeof(char *);

	/* Register topics */
	// Example
	// {
	//     "license":"asdf",
	//     "hostname":"abcd",
	//     "uuid": "1234",
	//     "agents": "MySQL1, MySQL2"
	// }
	zlog_info(scheduler_tag, "UUID and license");
	get_license(g_license);
	get_uuid(g_uuid);
	char reg_json[128], *ptr;
	ptr = reg_json;
	ptr += sprintf(ptr, "{\"license\":\"%s", g_license);
	// ptr += get_license(ptr);
	ptr += sprintf(ptr, "\",\"hostname\":\"");
	ptr += get_hostname(ptr);
	ptr += sprintf(ptr, "\",\"uuid\":\"%.*s", 36, g_uuid);
	// ptr += get_uuid(ptr);
	ptr += sprintf(ptr, "\",\"agents\":\"");
	ptr += get_agents(ptr);
	ptr += sprintf(ptr, "\"}");
	zlog_info(scheduler_tag, "Send");
	simple_send(global_sender, reg_json);

    /* Agents */
	zlog_info(scheduler_tag, "Initialize agents");
	agents = new_shash();
	if(!agents) {
		zlog_error(scheduler_tag, "Failed to create a agents hash");
		return;
	}

	/* MYSQL agent */
	for(int i=0; i<agent_number; ++i) {
		zlog_info(scheduler_tag, "Initialize agent \'%s\'", agent_names[i]);
		char conf_file[100];
		// TODO
		sprintf(conf_file, "conf/%s.yaml", "mysql");
		agent_t *agent = new_mysql_agent(agent_names[i], conf_file);
		if(!agent) {
			zlog_error(scheduler_tag, "Failed to create an agent");
			delete_shash(agents);
			return;
		}
		shash_insert(agents, agent_names[i], agent, ELEM_PTR);
		start(agent);
	}
}

void schedule() {
	initialize();
	while(1) {
		timestamp loop_start = get_timestamp();
		for(int i=0; i<agent_number; ++i) {
			agent_t *agent = (agent_t *)shash_search(agents, agent_names[i]);
			zlog_debug(scheduler_tag, "Status of \'%s\' [%c%c%c%c]", \
				agent_names[i], \
				outdated(agent)?'O':'.', \
				updating(agent)?'U':'.', \
				updating(agent)&&timeup(agent)?'T':'.', \
				buffer_full(agent)?'B':'.');
			if(buffer_full(agent))
				pack(agent);
			if(updating(agent)) {
				if(timeup(agent)) restart(agent);
			} else if(outdated(agent)) {
				poke(agent);
			}
		}

		if(TICK > get_timestamp()-loop_start) {
			timestamp remain_tick = TICK-(get_timestamp()-loop_start);
			zlog_debug(scheduler_tag, "Sleep for %fms", remain_tick/1000000.0);
			snyo_sleep(remain_tick);
		}
	}

	// Finishing
	finalize();
}

void finalize() {
	// TODO
}

int get_license(char *license) {
	return sprintf(license, "%s", "test");
}

int get_hostname(char *hostname) {
	if(!gethostname(hostname, 100))
		return strlen(hostname);
	return sprintf(hostname, "fail_to_get_hostname");
}

int get_uuid(char *uuid) {
	uuid_t out;
	uuid_generate(out);
	for(int i=0; i<sizeof(uuid_t); ++i) {
		if(i==4 || i==6 || i==8 || i==10)
			sprintf(uuid++, "-");
		uuid += sprintf(uuid, "%02x", *(out+i));
	}
	return 36;
}

int get_agents(char *agents) {
	int n = 0;
	for(int i=0; i<agent_number; ++i) {
		n += sprintf(agents+n, "%s", agent_names[i]);
		if(i < agent_number-1)
			sprintf(agents+n++, ",");
	}
	return n;
}
