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

#include <curl/curl.h>

size_t agent_number;

char *agent_names[] = {
	"mysql"
};

// TODO exception handling
void initialize() {
	/* Logging */
	scheduler_tag = (void *)zlog_get_category("scheduler");
    if (!scheduler_tag) return;

    /* Agents */
	zlog_info(scheduler_tag, "Initialize agents");
	agent_number = sizeof(agent_names)/sizeof(char *);
	agents = new_shash();
	if(!agents) {
		zlog_error(scheduler_tag, "Failed to create a agents shash");
		return;
	}

	/* MYSQL agent */
	zlog_info(scheduler_tag, "Initialize agent \'%s\'", agent_names[0]);
	agent_t *agent = new_mysql_agent("conf/mysql.yaml");
	if(!agent) {
		zlog_error(scheduler_tag, "Failed to create an agent");
		delete_shash(agents);
		return;
	}

	shash_insert(agents, "mysql", agent, ELEM_PTR);
	start(agent);

    /* Sender */
	zlog_info(scheduler_tag, "Initialize sender");
	global_sender = new_sender("http://192.168.121.14:8082");
	start_sender(global_sender);
}

void schedule() {
	initialize();

	while(1) {
		timestamp loop_start = get_timestamp();

		for(size_t i=0; i<agent_number; ++i) {
			agent_t *agent = get_agent(agent_names[i]);
			zlog_debug(scheduler_tag, "Check the status of \'%s\'", agent_names[i]);
			if(updating(agent)) {
				zlog_debug(scheduler_tag, "An agent is updating");
				if(timeup(agent)) {
					zlog_fatal(scheduler_tag, "Timeout");
					restart(agent);
				}
			} else if(outdated(agent)) {
				zlog_debug(scheduler_tag, "Outdated");
				poke(agent);
			}
		}

		timestamp remain_tick = TICK-(get_timestamp()-loop_start);
		zlog_debug(scheduler_tag, "Sleep for %lums", remain_tick/1000000);
		snyo_sleep(remain_tick);
	}

	// Finishing
	finalize();
}

void finalize() {
	// TODO
}