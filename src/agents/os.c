/** @file os_agent.c @author Snyo */

#include "agents/os.h"

#include "agent.h"
#include "snyohash.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#include <zlog.h>
#include <json/json.h>

const char *os_metric_names[] = {
	"disk_stat/disks",
	"net_stat/networks",
	"proc_stat/processes",
	"cpu_stat/number"
};

void collect_disk_metrics(json_object *values);
void collect_network_metrics(json_object *values);
void collect_proc_metrics(json_object *values);
void collect_cpu_metrics(json_object *values);

// TODO configuration parsing
agent_t *new_os_agent(const char *name, const char *conf) {
	agent_t *os_agent = new_agent(name, OS_AGENT_PERIOD);
	ASSERT(os_agent, delete_os_agent(os_agent), NULL, os_agent->tag, "Allocation fail");
	if(!os_agent) {
		zlog_error(os_agent->tag, "Failed to create an agent");
		return NULL;
	}

	os_agent->type = "os_linux_v1";
	os_agent->id   = "test";
	os_agent->agent_ip  = "test";
	os_agent->target_ip = "test";

	// os detail setup
	os_detail_t *detail = (os_detail_t *)malloc(sizeof(os_detail_t));
	if(!detail) {
		zlog_error(os_agent->tag, "Failed to allocate os detail");
		delete_agent(os_agent);
		return NULL;
	}

	// inheritance
	os_agent->detail = detail;

	// polymorphism
	os_agent->metric_names    = os_metric_names;
	os_agent->metric_names2   = json_object_new_object();
	os_agent->collect_metrics = collect_os_metrics;
	os_agent->fini = delete_os_agent;

	json_object *metric_arr = json_object_new_array();
	json_object_array_add(metric_arr, json_object_new_string("usage"));
	json_object_object_add(os_agent->metric_names2, "disk_stat", metric_arr);
	json_object *metric_arr2 = json_object_new_array();
	json_object_array_add(metric_arr2, json_object_new_string("byte_in"));
	json_object_array_add(metric_arr2, json_object_new_string("byte_out"));
	json_object_array_add(metric_arr2, json_object_new_string("packet_in"));
	json_object_array_add(metric_arr2, json_object_new_string("packet_out"));
	json_object_object_add(os_agent->metric_names2, "net_stat", metric_arr2);
	json_object *metric_arr3 = json_object_new_array();
	json_object_array_add(metric_arr3, json_object_new_string("user"));
	json_object_array_add(metric_arr3, json_object_new_string("cpu_use"));
	json_object_array_add(metric_arr3, json_object_new_string("mem_use"));
	json_object_object_add(os_agent->metric_names2, "proc_stat", metric_arr3);
	json_object *metric_arr4 = json_object_new_array();
	json_object_object_add(os_agent->metric_names2, "cpu_stat", metric_arr4);

	return os_agent;
}

void collect_os_metrics(agent_t *os_agent) {
	zlog_debug(os_agent->tag, "Collecting metrics");

	json_object *values = json_object_new_array();
	collect_disk_metrics(values);
	collect_network_metrics(values);
	collect_proc_metrics(values);
	collect_cpu_metrics(values);

	add_metrics(os_agent, values);
}

void delete_os_agent(agent_t *os_agent) {
	free(os_agent->detail);
	delete_agent(os_agent);
}

void collect_disk_metrics(json_object *values) {
	/* Disk usage */
	FILE *df_fp = popen("df -T -P | grep ext | awk \'{sub(\"%%\", \"\", $6); print $1, $6}\'", "r");
	char disk_name[100];
	int disk_usage;
	json_object *disk_json = json_object_new_object();
	while(fscanf(df_fp, "%s%d", disk_name, &disk_usage) == 2) {
		json_object *disk_info = json_object_new_array();
		json_object_array_add(disk_info, json_object_new_double((double)disk_usage/100));
		json_object_object_add(disk_json, disk_name, disk_info);
	}
	json_object_array_add(values, disk_json);
	pclose(df_fp);
}

void collect_network_metrics(json_object *values) {
	/* Network */
	FILE *net_fp = popen("cat /proc/net/dev | grep : | awk \'{sub(\":\", \"\", $1); print $1, $2, $3, $10, $11}\'", "r");
	char net_name[100];
	int byte_in, byte_out;
	int pckt_in, pckt_out;
	json_object *net_json = json_object_new_object();
	while(fscanf(net_fp, "%s%d%d%d%d", net_name, &byte_in, &byte_out, &pckt_in, &pckt_out) == 5) {
		json_object *net_info = json_object_new_array();
		/* Bytes in and out */
		json_object_array_add(net_info, json_object_new_int(byte_in));
		json_object_array_add(net_info, json_object_new_int(byte_out));
		/* Packets in and out */
		json_object_array_add(net_info, json_object_new_int(pckt_in));
		json_object_array_add(net_info, json_object_new_int(pckt_out));
		json_object_object_add(net_json, net_name, net_info);
	}
	json_object_array_add(values, net_json);
	pclose(net_fp);
}

void collect_proc_metrics(json_object *values) {
	/* Top 3 CPU using processes */
	FILE *top_fp = popen("top -n 1 | grep -A 3 PID | awk \'{print $3, $10, $11, $13}\'", "r");
	char tmp[100];
	json_object *proc_json = json_object_new_object();
	if(fgets(tmp, 100, top_fp)) {
		char user[100], name[100];
		double cpu, mem;
		while(fscanf(top_fp, "%s%lf%lf%s", user, &cpu, &mem, name) == 4) {
			json_object *proc_info = json_object_new_array();
			json_object_array_add(proc_info, json_object_new_string(user));
			json_object_array_add(proc_info, json_object_new_double(cpu/100));
			json_object_array_add(proc_info, json_object_new_double(mem/100));
			json_object_object_add(proc_json, name, proc_info);
		}
		json_object_array_add(values, proc_json);
	}
	pclose(top_fp);
}

void collect_cpu_metrics(json_object *values) {
	/* CPU numbers */
	FILE *lscpu_fp = popen("lscpu | grep \'^CPU(s):\' | awk \'{print $2}\'", "r");
	int cpu_num;
	while(fscanf(lscpu_fp, "%d", &cpu_num) == 1) {
		json_object_array_add(values, json_object_new_int(cpu_num));
	}
	pclose(lscpu_fp);
}