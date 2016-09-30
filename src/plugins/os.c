/**
 * @file os.c
 * @author Snyo
 */
#include "plugins/os.h"

#include <stdio.h>
#include <string.h>

#include <zlog.h>

#include "pluggable.h"
#include "util.h"

#define OS_PLUGIN_TICK NS_PER_S/2

const char *network_metric_names[] = {
	"net_stat/byte_in",
	"net_stat/byte_out",
	"net_stat/packet_in",
	"net_stat/packet_out"
};

const char *disk_metric_names[] = {
	"disk_stat/IOPs",
	"disk_stat/average_queue",
	"disk_stat/await",
	"disk_stat/service_time"
};

const char *proc_metric_names[] = {
	"proc_stat/user",
	"proc_stat/cpu_usage",
	"proc_stat/mem_usage"
};

int collect_cpu_metrics(plugin_t *plugin, json_object *values);
int collect_disk_metrics(plugin_t *plugin, json_object *values);
int collect_proc_metrics(plugin_t *plugin, json_object *values);
int collect_memory_metrics(plugin_t *plugin, json_object *values);
int collect_network_metrics(plugin_t *plugin, json_object *values);

// TODO configuration parsing
os_plugin_t *new_os_plugin() {
	os_plugin_t *plugin = new_plugin(OS_PLUGIN_TICK);
	if(!plugin) return NULL;

	plugin->tag = zlog_get_category("Agent_os");
	if(!plugin->tag) return NULL;

	plugin->num   = 1;
	plugin->agent_ip = "test";
	plugin->target_ip = "test";

	plugin->holding = 0;
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
	json_object *values = json_object_new_array();

	int metric_number = 0;
	metric_number += collect_network_metrics(plugin, values);
	metric_number += collect_cpu_metrics(plugin, values);
	metric_number += collect_disk_metrics(plugin, values);
	// metric_number += collect_proc_metrics(plugin, values);
	metric_number += collect_memory_metrics(plugin, values);

	if(!*(int *)plugin->spec)
		*(int *)plugin->spec = metric_number;

	char ts[100];
	sprintf(ts, "%lu", plugin->next_run - plugin->period);
	json_object_object_add(plugin->values, ts, values);
	++plugin->holding;

	if(plugin->holding == plugin->full_count) 
		pack(plugin);
}

int collect_network_metrics(plugin_t *plugin, json_object *values) {
	FILE *net_fp = popen("cat /proc/net/dev | grep : | awk \'{sub(\":\", \"\", $1); print $1, $2, $3, $10, $11}\'", "r");
	if(!net_fp) return 0;

	char net_name[50];
	int byte_in, byte_out;
	int pckt_in, pckt_out;

	int collected = 0;
	while(fscanf(net_fp, "%s", net_name) == 1) {
		if(fscanf(net_fp, "%d%d%d%d", &byte_in, &byte_out, &pckt_in, &pckt_out) == 4) {
			if(!*(int *)plugin->spec) {
				for(int i=0; i<sizeof(network_metric_names)/sizeof(char *); ++i) {
					char new_metric[150];
					sprintf(new_metric, "%s/%s", network_metric_names[i], net_name);
					json_object_array_add(plugin->metric_names, json_object_new_string(new_metric));
					collected++;
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

	return collected;
}

int collect_cpu_metrics(plugin_t *plugin, json_object *values) {
	FILE *pipe = popen("lscpu | grep \'^CPU(s):\' | awk \'{print $2}\'", "r");
	if(!pipe) return 0;

	int cpu_num;

	int collected = 0;
	while(fscanf(pipe, "%d", &cpu_num) == 1) {
		if(!*(int *)plugin->spec) {
			json_object_array_add(plugin->metric_names, json_object_new_string("cpu_stat/num"));
			collected ++;
		}
		json_object_array_add(values, json_object_new_int(cpu_num));
	}
	pclose(pipe);

	return collected;
}

int collect_disk_metrics(plugin_t *plugin, json_object *values) {
	FILE *pipe = popen("df -T -P | grep ext | awk \'{sub(\"%%\", \"\", $6); print $1, $6}\'", "r");
	if(!pipe) return 0;

	char disk_name[100];
	int disk_usage;

	int collected = 0;
	while(fscanf(pipe, "%s", disk_name) == 1) {
		if(fscanf(pipe, "%d", &disk_usage) == 1) {
			if(!*(int *)plugin->spec) {
				char new_metric[150];
				sprintf(new_metric, "disk_stat/disk_usage/%s", disk_name);
				json_object_array_add(plugin->metric_names, json_object_new_string(new_metric));
				collected ++;
			}
			json_object_array_add(values, json_object_new_double((double)disk_usage/100));
		} else break;
	}
	pclose(pipe);

	pipe = popen("iostat -xc | grep -A 20 Device: | awk '{print $1, $4, $5, $9, $10, $13}'", "r");
	float rps, wps, avgqu, await, svctm;
	if(fscanf(pipe, "%*[^\n]\n") != 0)
		return collected;
	while(fscanf(pipe, "%s", disk_name) == 1) {
		if(fscanf(pipe, "%f%f%f%f%f", &rps, &wps, &avgqu, &await, &svctm) == 5) {
			if(!*(int *)plugin->spec) {
				for(int i=0; i<sizeof(disk_metric_names)/sizeof(char *); ++i) {
					char new_metric[150];
					sprintf(new_metric, "%s/%s", disk_metric_names[i], disk_name);
					json_object_array_add(plugin->metric_names, json_object_new_string(new_metric));
					collected ++;
				}
			}
			json_object_array_add(values, json_object_new_double(rps+wps));
			json_object_array_add(values, json_object_new_double(avgqu));
			json_object_array_add(values, json_object_new_double(await));
			json_object_array_add(values, json_object_new_double(svctm));
		} else break;
	}

	pclose(pipe);

	return collected;
}

int collect_proc_metrics(plugin_t *plugin, json_object *values) {
	FILE *pipe = popen("top -n 1 | grep -A 3 PID | awk \'{print $3, $10, $11, $13}\'", "r");
	if(!pipe) return 0;

	char tmp[100];
	// fscanf(pipe, "%*[^\n]\n");

	int collected = 0;

	if(fgets(tmp, 100, pipe)) {
		char user[100], name[100];
		double cpu, mem;
		while(fscanf(pipe, "%s%lf%lf%s", user, &cpu, &mem, name) == 4) {
			printf("%s %lf %lf %s\n", user, cpu, mem, name);
		}
	}
	pclose(pipe);

	return collected;
}

int collect_memory_metrics(plugin_t *plugin, json_object *values) {
	FILE *pipe = popen("cat /proc/meminfo | grep -E \"^Mem|^Cached|^Active:|^Inactive:|^Vmalloc[TU]\" | awk '{print $2}'", "r");
	if(!pipe) return 0;

	int total, free, available, cached;
	int active, inactive;
	int v_total, v_used;

	int collected = 0;

	if(fscanf(pipe, "%d%d%d%d%d%d%d%d", &total, &free, &available, &cached, &active, &inactive, &v_total, &v_used) != 8)
		return 0;

	if(!*(int *)plugin->spec) {
		json_object_array_add(plugin->metric_names, json_object_new_string("mem_stat/total"));
		json_object_array_add(plugin->metric_names, json_object_new_string("mem_stat/free"));
		json_object_array_add(plugin->metric_names, json_object_new_string("mem_stat/available"));
		json_object_array_add(plugin->metric_names, json_object_new_string("mem_stat/cached"));
		json_object_array_add(plugin->metric_names, json_object_new_string("mem_stat/user"));
		json_object_array_add(plugin->metric_names, json_object_new_string("mem_stat/sys"));
		json_object_array_add(plugin->metric_names, json_object_new_string("mem_stat/virtual_usage"));
	}
	json_object_array_add(values, json_object_new_int(total));
	json_object_array_add(values, json_object_new_int(free));
	json_object_array_add(values, json_object_new_int(available));
	json_object_array_add(values, json_object_new_int(cached));
	json_object_array_add(values, json_object_new_int((active+inactive)));
	json_object_array_add(values, json_object_new_int((total-free-active-inactive)));
	json_object_array_add(values, json_object_new_int((double)v_used/(double)v_total));
	collected += 7;

	pclose(pipe);

	return collected;
}