/** @file network_agent.c @author Snyo */

#include "agents/network.h"

#include "agent.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#include <zlog.h>
#include <json/json.h>

const char *network_metric_names[] = {
	"net_stat/name",
	"net_stat/byte_in",
	"net_stat/byte_out",
	"net_stat/packet_in",
	"net_stat/packet_out"
};

// TODO configuration parsing
agent_t *new_network_agent(const char *name, const char *conf) {
	agent_t *agent = new_agent(name, NETWORK_AGENT_PERIOD);
	if(!agent) return NULL;

	// network detail setup
	network_detail_t *detail = (network_detail_t *)malloc(sizeof(network_detail_t));
	if(!detail) {
		zlog_error(agent->tag, "Failed to allocate network detail");
		delete_agent(agent);
		return NULL;
	}

	agent->type = "linux_linux_1.0";
	agent->id   = 1;
	agent->agent_ip  = "test";
	agent->target_ip = "test";

	// inheritance
	agent->detail = detail;

	// polymorphism
	agent->metric_names    = network_metric_names;
	agent->collect_metrics = collect_network_metrics;
	agent->delete = delete_agent;

	return agent;
}

void collect_network_metrics(agent_t *agent) {
	zlog_debug(agent->tag, "Collecting metrics");

	json_object *values = json_object_new_array();
	/* Network */
	FILE *net_fp = popen("cat /proc/net/dev | grep : | awk \'{sub(\":\", \"\", $1); print $1, $2, $3, $10, $11}\'", "r");
	char net_name[100];
	int byte_in, byte_out;
	int pckt_in, pckt_out;
	json_object *net_json = json_object_new_object();
	json_object *net_info = json_object_new_array();
	if(fscanf(net_fp, "%s%d%d%d%d", net_name, &byte_in, &byte_out, &pckt_in, &pckt_out) == 5) {
		// json_object *net_info = json_object_new_array();
		// // bytes in and out
		// json_object_array_add(net_info, json_object_new_int(byte_in));
		// json_object_array_add(net_info, json_object_new_int(byte_out));
		// // packets in and out
		// json_object_array_add(net_info, json_object_new_int(pckt_in));
		// json_object_array_add(net_info, json_object_new_int(pckt_out));
		// json_object_object_add(net_json, net_name, net_info);

		// json_object_array_add(net_info, json_object_new_int(byte_in));
		// json_object_array_add(net_info, json_object_new_int(byte_out));
		// json_object_array_add(values, net_info);
		json_object_array_add(values, json_object_new_string(net_name));
		// bytes in and out
		json_object_array_add(values, json_object_new_int(byte_in));
		json_object_array_add(values, json_object_new_int(byte_out));
		// packets in and out
		json_object_array_add(values, json_object_new_int(pckt_in));
		json_object_array_add(values, json_object_new_int(pckt_out));
	}
	// json_object_array_add(values, net_json);
	pclose(net_fp);

	add_metrics(agent, values);
}

void delete_network_agent(agent_t *agent) {
	free(agent->detail);
	delete_agent(agent);
}