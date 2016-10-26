/**
 * @file os.c
 * @author Snyo
 */
#include "plugins/os.h"

#include <stdio.h>

#include "pluggable.h"
#include "util.h"

#define OS_PLUGIN_TICK NS_PER_S*3
#define OS_PLUGIN_FULL 20

const char *network_metric_names[] = {
	"net_stat_recv_byte",
	"net_stat_recv_packet",
	"net_stat_recv_error",
	"net_stat_send_byte",
	"net_stat_send_packet",
	"net_stat_send_error"
};

const char *disk_metric_names[] = {
	"disk_stat_iops",
	"disk_stat_rps",
	"disk_stat_wps",
	"disk_stat_average_queue",
	"disk_stat_await",
	"disk_stat_service_time"
};

static void collect_cpu_metrics(plugin_t *plugin, json_object *values);
static void collect_disk_metrics(plugin_t *plugin, json_object *values);
static void collect_proc_metrics(plugin_t *plugin, json_object *values);
static void collect_memory_metrics(plugin_t *plugin, json_object *values);
static void collect_network_metrics(plugin_t *plugin, json_object *values);

int os_plugin_init(plugin_t *plugin) {
	plugin->spec = malloc(sizeof(1));
	if(!plugin->spec) return -1;
	*(unsigned *)plugin->spec = 0;

	plugin->target_ip  = "test";
	plugin->period     = OS_PLUGIN_TICK;
	plugin->full_count = OS_PLUGIN_FULL;

	plugin->collect    = collect_os_metrics;
	plugin->fini       = os_plugin_fini;

	return 0;
}

void os_plugin_fini(plugin_t *plugin) {
	if(!plugin)
		return;
	
	if(plugin->spec)
		free(plugin->spec);
}

void collect_os_metrics(plugin_t *plugin) {
	json_object *values = json_object_new_array();

	collect_network_metrics(plugin, values);
	collect_cpu_metrics(plugin, values);
	collect_disk_metrics(plugin, values);
	collect_proc_metrics(plugin, values);
	collect_memory_metrics(plugin, values);
	*(unsigned *)plugin->spec = 1;

	if(plugin->holding+1 == plugin->full_count)
		*(unsigned *)plugin->spec = 0;

	char ts[20];
	snprintf(ts, 20, "%lu", plugin->next_run - plugin->period);
	json_object_object_add(plugin->values, ts, values);
}

void collect_network_metrics(plugin_t *plugin, json_object *values) {
	FILE *pipe = popen("cat /proc/net/dev | tail -n -2 | awk '{sub(\":\",\"\",$1); print $1,$2,$3,$4,$10,$11,$12}'", "r");
	if(!pipe) return;

	char net_name[50];
	int recv_byte, recv_pckt, recv_err;
	int send_byte, send_pckt, send_err;

	int total_recv_byte = 0;
	int total_send_byte = 0;
	while(fscanf(pipe, "%s", net_name) == 1) {
		if(fscanf(pipe, "%d%d%d%d%d%d", &recv_byte, &recv_pckt, &recv_err, &send_byte, &send_pckt, &send_err) == 6) {
			if(!*(unsigned *)plugin->spec) {
				for(int i=0; i<sizeof(network_metric_names)/sizeof(char *); ++i) {
					char new_metric[100];
					snprintf(new_metric, 100, "%s|%s", network_metric_names[i], net_name);
					json_object_array_add(plugin->metric_names, json_object_new_string(new_metric));
				}
			}
			total_recv_byte += recv_byte;
			total_send_byte += send_byte;
			json_object_array_add(values, json_object_new_int(recv_byte));
			json_object_array_add(values, json_object_new_int(recv_pckt));
			json_object_array_add(values, json_object_new_int(recv_err));
			json_object_array_add(values, json_object_new_int(send_byte));
			json_object_array_add(values, json_object_new_int(send_pckt));
			json_object_array_add(values, json_object_new_int(send_err));
		} else break;
	}
	if(!*(unsigned *)plugin->spec) {
		json_object_array_add(plugin->metric_names, json_object_new_string("net_stat_total_recv_byte"));
		json_object_array_add(plugin->metric_names, json_object_new_string("net_stat_total_send_byte"));
	}
	json_object_array_add(values, json_object_new_int(total_recv_byte));
	json_object_array_add(values, json_object_new_int(total_send_byte));
	pclose(pipe);
}

void collect_cpu_metrics(plugin_t *plugin, json_object *values) {
	FILE *pipe;
	pipe = popen("iostat -c | tail -n 2 | head -n 1 | awk '{print $1,$3,$6}'", "r");
	if(!pipe) return;

	float cpu_user, cpu_sys, cpu_idle;
	if(fscanf(pipe, "%f%f%f", &cpu_user, &cpu_sys, &cpu_idle) != 3)
		return;
	if(!*(unsigned *)plugin->spec) {
		json_object_array_add(plugin->metric_names, json_object_new_string("cpu_stat_user"));
		json_object_array_add(plugin->metric_names, json_object_new_string("cpu_stat_sys"));
		json_object_array_add(plugin->metric_names, json_object_new_string("cpu_stat_idle"));
	}
	json_object_array_add(values, json_object_new_double(cpu_user));
	json_object_array_add(values, json_object_new_double(cpu_sys));
	json_object_array_add(values, json_object_new_double(cpu_idle));

	pclose(pipe);
}

void collect_disk_metrics(plugin_t *plugin, json_object *values) {
	FILE *pipe;
	pipe = popen("lsblk -nbrso 'NAME,TYPE,MOUNTPOINT'\
			      | awk '$2==\"part\"{if($3!=\"\"&&$3!=\"[SWAP]\")print $1,$3;}'", "r");
	if(!pipe) return;

	char part_name[7];
	char part_mount[30];
	while(fscanf(pipe, "%s%s\n", part_name, part_mount) == 2) {
		/* Disk usage with df */
		char df_cmd[50];
		snprintf(df_cmd, 50, "df | awk '$1~/%s/{print $2,$3}'", part_name);
		FILE *df_pipe = popen(df_cmd, "r");
		if(df_pipe) {
			unsigned int part_total, part_used;
			if(fscanf(df_pipe, "%u%u", &part_total, &part_used) == 2) {
				if(!*(unsigned *)plugin->spec) {
					char metric_name[50];
					snprintf(metric_name, 50, "disk_stat_part_total|%s(%s)", part_name, part_mount);
					json_object_array_add(plugin->metric_names, json_object_new_string(metric_name));
					snprintf(metric_name, 50, "disk_stat_part_used|%s(%s)", part_name, part_mount);
					json_object_array_add(plugin->metric_names, json_object_new_string(metric_name));
					snprintf(metric_name, 50, "disk_stat_part_usage|%s(%s)", part_name, part_mount);
					json_object_array_add(plugin->metric_names, json_object_new_string(metric_name));
				}
				json_object_array_add(values, json_object_new_double(part_total));
				json_object_array_add(values, json_object_new_double(part_used));
				json_object_array_add(values, json_object_new_double((float)part_used/(float)part_total));
			}
			pclose(df_pipe);
		}

		/* Other values with iostat */
		char iostat_cmd[60]; 
		snprintf(iostat_cmd, 60, "iostat -xpd | awk '$1~/^%s$/{print $4,$5,$9,$10,$13}'", part_name);
		FILE *iostat_fd = popen(iostat_cmd, "r");
		float total_iops = 0;
		if(iostat_fd) {
			float rps, wps, avgqu, await, svctm;
			if(fscanf(iostat_fd, "%f%f%f%f%f", &rps, &wps, &avgqu, &await, &svctm) == 5) {
				if(!*(unsigned *)plugin->spec) {
					for(int i=0; i<sizeof(disk_metric_names)/sizeof(char *); ++i) {
						char metric_name[50];
						snprintf(metric_name, 50, "%s|%s(%s)", disk_metric_names[i], part_name, part_mount);
						json_object_array_add(plugin->metric_names, json_object_new_string(metric_name));
					}
				}
				total_iops += rps + wps;
				json_object_array_add(values, json_object_new_double(rps+wps));
				json_object_array_add(values, json_object_new_double(rps));
				json_object_array_add(values, json_object_new_double(wps));
				json_object_array_add(values, json_object_new_double(avgqu));
				json_object_array_add(values, json_object_new_double(await));
				json_object_array_add(values, json_object_new_double(svctm));
			}
			if(!*(unsigned *)plugin->spec) {
				json_object_array_add(plugin->metric_names, json_object_new_string("disk_stat_total_iops"));
			}
			json_object_array_add(values, json_object_new_double(total_iops));
			pclose(iostat_fd);
		}
	}
	pclose(pipe);
}

void collect_proc_metrics(plugin_t *plugin, json_object *values) {
	/* Top N metrics */
	FILE *pipe = popen("ps -eo pcpu,comm --sort=-pcpu,-pmem --no-headers | head -n 5", "r");
	if(!pipe) return;

	char name[100];
	double stat;
	for(int i=0; i<5; ++i) {
		if(fscanf(pipe, "%lf%s\n", &stat, name) == 2) {
			if(!*(unsigned *)plugin->spec) {
				char metric_name[50];
				snprintf(metric_name, 50, "proc_stat_cpu_top%d", i+1);
				json_object_array_add(plugin->metric_names, json_object_new_string(metric_name));
			}
			json_object_array_add(values, json_object_new_string(name));
		}
	}
	pclose(pipe);

	pipe = popen("ps -eo pmem,comm --sort=-pmem,-pcpu --no-headers | head -n 5", "r");
	if(!pipe) return;
	for(int i=0; i<5; ++i) {
		if(fscanf(pipe, "%lf%s\n", &stat, name) == 2) {
			if(!*(unsigned *)plugin->spec) {
				char metric_name[50];
				snprintf(metric_name, 50, "proc_stat_mem_top%d", i+1);
				json_object_array_add(plugin->metric_names, json_object_new_string(metric_name));
			}
			json_object_array_add(values, json_object_new_string(name));
		}
	}
	pclose(pipe);
}

void collect_memory_metrics(plugin_t *plugin, json_object *values) {
	FILE *pipe = popen("cat /proc/meminfo | awk '/Mem|^Cached|Active:|Inactive:|Vmalloc[TU]/{print $2}'", "r");
	if(!pipe) return;

	unsigned long total, free, available, cached, active, inactive, v_total, v_used;
	if(fscanf(pipe, "%lu%lu%lu%lu%lu%lu%lu%lu", &total, &free, &available, &cached, &active, &inactive, &v_total, &v_used) != 8)
		return;

	if(!*(unsigned *)plugin->spec) {
		json_object_array_add(plugin->metric_names, json_object_new_string("mem_stat_total"));
		json_object_array_add(plugin->metric_names, json_object_new_string("mem_stat_free"));
		json_object_array_add(plugin->metric_names, json_object_new_string("mem_stat_available"));
		json_object_array_add(plugin->metric_names, json_object_new_string("mem_stat_cached"));
		json_object_array_add(plugin->metric_names, json_object_new_string("mem_stat_user"));
		json_object_array_add(plugin->metric_names, json_object_new_string("mem_stat_sys"));
		json_object_array_add(plugin->metric_names, json_object_new_string("mem_stat_virtual_usage"));
	}
	json_object_array_add(values, json_object_new_int(total));
	json_object_array_add(values, json_object_new_int(free));
	json_object_array_add(values, json_object_new_int(available));
	json_object_array_add(values, json_object_new_int(cached));
	json_object_array_add(values, json_object_new_int((active+inactive)));
	json_object_array_add(values, json_object_new_int((total-free-active-inactive)));
	json_object_array_add(values, json_object_new_double((double)v_used/(double)v_total));

	pclose(pipe);
}
