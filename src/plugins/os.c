/**
 * @file os.c
 * @author Snyo
 */
#include "plugins/os.h"

#include "pluggable.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#include <zlog.h>

#define OS_PLUGIN_TICK NS_PER_S/2

const char *network_metric_names[] = {
	"net_stat/byte_in",
	"net_stat/byte_out",
	"net_stat/packet_in",
	"net_stat/packet_out"
};

// TODO configuration parsing
os_plugin_t *new_os_plugin() {
	os_plugin_t *plugin = new_plugin(OS_PLUGIN_TICK);
	if(!plugin) return NULL;

	plugin->tag = zlog_get_category("Agent_os");
	if(!plugin->tag) return NULL;

	plugin->num   = 1;
	plugin->agent_ip = "test";
	plugin->target_ip = "test";

	plugin->stored = 0;
	plugin->full_count = 5;

	// inheritance
	plugin->spec = malloc(1);
	if(!plugin->spec) return NULL;
	*(int *)plugin->spec = 0;

	// polymorphism
	plugin->job = collect_os_metrics;
	plugin->delete = delete_os_plugin;

	return plugin;
}

void delete_os_plugin(os_plugin_t *plugin) {
	free(plugin->spec);
	delete_plugin(plugin);
}

void collect_os_metrics(os_plugin_t *plugin) {
	zlog_debug(plugin->tag, "Collecting metrics");

	/* Network */
	FILE *net_fp = popen("cat /proc/net/dev | grep : | awk \'{sub(\":\", \"\", $1); print $1, $2, $3, $10, $11}\'", "r");
	char net_name[100];
	int byte_in, byte_out;
	int pckt_in, pckt_out;

	json_object *values = json_object_new_array();
	int metric_number = 0;
	while(fscanf(net_fp, "%s", net_name) == 1) {
		if(fscanf(net_fp, "%d%d%d%d", &byte_in, &byte_out, &pckt_in, &pckt_out) == 4) {
			if(!*(int *)plugin->spec) {
				for(int i=0; i<sizeof(network_metric_names)/sizeof(char *); ++i) {
					char new_metric[1024];
					sprintf(new_metric, "%s/%s", network_metric_names[i], net_name);
					json_object_array_add(plugin->metric_names, json_object_new_string(new_metric));
					metric_number ++;
				}
			}
			// bytes in and out
			json_object_array_add(values, json_object_new_int(byte_in));
			json_object_array_add(values, json_object_new_int(byte_out));
			// packets in and out
			json_object_array_add(values, json_object_new_int(pckt_in));
			json_object_array_add(values, json_object_new_int(pckt_out));
		} else break;
	}
	pclose(net_fp);

	/* CPU numbers */
	FILE *cpu_fp = popen("lscpu | grep \'^CPU(s):\' | awk \'{print $2}\'", "r");
	int cpu_num;
	while(fscanf(cpu_fp, "%d", &cpu_num) == 1) {
		if(!*(int *)plugin->spec) {
			json_object_array_add(plugin->metric_names, json_object_new_string("cpu_stat/num"));
			metric_number ++;
		}
		json_object_array_add(values, json_object_new_int(cpu_num));
	}
	pclose(cpu_fp);

	/* Disk usage */
	FILE *df_fp = popen("df -T -P | grep ext | awk \'{sub(\"%%\", \"\", $6); print $1, $6}\'", "r");
	char disk_name[100];
	int disk_usage;
	while(fscanf(df_fp, "%s", disk_name) == 1) {
		if(fscanf(df_fp, "%d", &disk_usage) == 1) {
			if(!*(int *)plugin->spec) {
				char new_metric[1024];
				sprintf(new_metric, "disk_stat/disk_usage/%s", disk_name);
				json_object_array_add(plugin->metric_names, json_object_new_string(new_metric));
				metric_number ++;
			}
			json_object_array_add(values, json_object_new_double((double)disk_usage/100));
		} else break;
	}
	pclose(df_fp);

	/* Top 3 CPU using processes */
	// FILE *top_fp = popen("top -n 1 | grep -A 3 PID | awk \'{print $3, $10, $11, $13}\'", "r");
	// char tmp[100];
	// fscanf(top_fp, "%*[^\n]\n");
	// json_object *proc_json = json_object_new_object();
	// if(fgets(tmp, 100, top_fp)) {
	// 	char user[100], name[100];
	// 	double cpu, mem;
	// 	while(fscanf(top_fp, "%s%lf%lf%s", user, &cpu, &mem, name) == 4) {
	// 		printf("%s %lf %lf %s\n", user, cpu, mem, name);
	// 		json_object *proc_info = json_object_new_array();
	// 		json_object_array_add(proc_info, json_object_new_string(user));
	// 		json_object_array_add(proc_info, json_object_new_double(cpu/100));
	// 		json_object_array_add(proc_info, json_object_new_double(mem/100));
	// 		json_object_object_add(proc_json, name, proc_info);
	// 	}
	// 	json_object_array_add(values, proc_json);
	// }
	// pclose(top_fp);

	if(!*(int *)plugin->spec)
		*(int *)plugin->spec = metric_number;

	char ts[100];
	sprintf(ts, "%lu", plugin->next_run - plugin->period);
	json_object_object_add(plugin->values, ts, values);
	++plugin->stored;

	if(plugin->stored == plugin->full_count) {
		pack(plugin);
		plugin->metric_names = json_object_new_array();
		plugin->values = json_object_new_object();
		*(int *)plugin->spec = 0;
		plugin->stored = 0;
	}
}

const char *proc_metric_names[] = {
	"proc_stat/user",
	"proc_stat/cpu_usage",
	"proc_stat/mem_usage"
};