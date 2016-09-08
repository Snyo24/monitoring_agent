/** @file os_agent.c @author Snyo */

#include "agents/os.h"

#include "agent.h"
#include "snyohash.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#include <zlog.h>
#include <json/json.h>

char *os_metric_names[] = {
	"disk/usage",
	"disk/number",
	"network/byte_in",
	"network/byte_out",
	"network/packet_in",
	"network/packet_out",
	"network/byte_in_per_sec",
	"network/byte_out_per_sec",
	"network/number"
};

// TODO configuration parsing
agent_t *os_agent_init(const char *name, const char *conf) {
	agent_t *os_agent = agent_init(name, OS_AGENT_PERIOD);
	if(!os_agent) {
		zlog_error(os_agent->tag, "Failed to create an agent");
		return NULL;
	}

	// os detail setup
	os_detail_t *detail = (os_detail_t *)malloc(sizeof(os_detail_t));
	if(!detail) {
		zlog_error(os_agent->tag, "Failed to allocate os detail");
		agent_fini(os_agent);
		return NULL;
	}
	detail->last_byte_in = -1;
	detail->last_byte_out = -1;

	// inheritance
	os_agent->detail = detail;

	// polymorphism
	os_agent->metric_number   = sizeof(os_metric_names)/sizeof(char *);
	os_agent->metric_names    = os_metric_names;
	os_agent->collect_metrics = collect_os_metrics;

	os_agent->fini = os_agent_fini;

	return os_agent;
}

void collect_os_metrics(agent_t *os_agent) {
	zlog_debug(os_agent->tag, "Collecting metrics");
	json_object *values = json_object_new_array();
	os_detail_t *os_detail = (os_detail_t *)os_agent->detail;

	/* Disk usage */
	FILE *df_fp = popen("df -T -P | grep ext | awk \'{gsub(\"%%\", \"\"); print $6}\'", "r");
	int disk_num = 0;
	int disk_usage;

	if(fscanf(df_fp, "%d", &disk_usage) == 1) {
		disk_num++;
		json_object_array_add(values, json_object_new_double((double)disk_usage/100));
	}
	json_object_array_add(values, json_object_new_int(disk_num));
	pclose(df_fp);

	/* Network */
	FILE *net_fp = popen("cat /proc/net/dev | grep : | wc -l", "r");
	int socket_num;
	if(fscanf(net_fp, "%d", &socket_num) == 1)
		json_object_array_add(values, json_object_new_int(socket_num));
	pclose(net_fp);

	net_fp = popen("cat /proc/net/dev | grep : | awk \'{print $2, $3, $10, $11}\'", "r");
	int byte_in, byte_out;
	int pckt_in, pckt_out;
	if(fscanf(net_fp, "%d%d%d%d", &byte_in, &byte_out, &pckt_in, &pckt_out) == 4) {
		/* Bytes in and out */
		json_object_array_add(values, json_object_new_int(byte_in));
		json_object_array_add(values, json_object_new_int(byte_out));
		/* Packets in and out */
		json_object_array_add(values, json_object_new_int(pckt_in));
		json_object_array_add(values, json_object_new_int(pckt_out));
		/* Rate in and out */
		double rate;
		rate = os_detail->last_byte_in<0 ? 0 : (double)(byte_in-os_detail->last_byte_in)/OS_AGENT_PERIOD;
		json_object_array_add(values, json_object_new_double(rate));
		rate = os_detail->last_byte_out<0 ? 0 : (double)(byte_out-os_detail->last_byte_out)/OS_AGENT_PERIOD;
		json_object_array_add(values, json_object_new_double(rate));

		os_detail->last_byte_in = byte_in;
		os_detail->last_byte_out = byte_out;
	}
	pclose(net_fp);
	add(os_agent, values);
}

void os_agent_fini(agent_t *os_agent) {
	free(os_agent->detail);
	agent_fini(os_agent);
}