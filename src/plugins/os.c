/**
 * @file os.c
 * @author Snyo
 */
#include "plugins/os.h"

#include <stdio.h>
#include <string.h>

#include "plugin.h"

#define OS_PLUGIN_TICK 3
#define OS_PLUGIN_CAPACITY 10

typedef struct _os_plugin {
	plugin_t;

	unsigned gathered : 1;
} os_plugin_t;

const char *network_metric[] = {
	"net_stat_recv_byte",
	"net_stat_recv_packet",
	"net_stat_recv_error",
	"net_stat_send_byte",
	"net_stat_send_packet",
	"net_stat_send_error"
};

const char *disk_metric[] = {
	"disk_stat_io",
	"disk_stat_iotime",
	"disk_stat_reads",
	"disk_stat_read_sector",
	"disk_stat_read_time",
	"disk_stat_writes",
	"disk_stat_write_sector",
	"disk_stat_write_time",
	"disk_stat_weight"
};

static void _collect_cpu(plugin_t *plugin, json_object *values);
static void _collect_disk(plugin_t *plugin, json_object *values);
static void _collect_proc(plugin_t *plugin, json_object *values);
static void _collect_memory(plugin_t *plugin, json_object *values);
static void _collect_network(plugin_t *plugin, json_object *values);

static void collect_os(plugin_t *plugin);

int os_plugin_init(plugin_t *plugin, char *option) {
	plugin->spec = malloc(sizeof(1));
	if(!plugin->spec) return -1;
	*(unsigned *)plugin->spec = 0;

	plugin->type     = "linux_linux_1.0";
	plugin->ip       = 0;
	plugin->period   = OS_PLUGIN_TICK;
	plugin->capacity = OS_PLUGIN_CAPACITY;

	plugin->collect  = collect_os;
	plugin->fini     = os_plugin_fini;

	return 0;
}

void os_plugin_fini(plugin_t *plugin) {
	if(!plugin)
		return;

	if(plugin->spec)
		free(plugin->spec);
}

void collect_os(plugin_t *plugin) {
	json_object *values = json_object_new_array();

	_collect_cpu(plugin, values);
	_collect_disk(plugin, values);
	_collect_proc(plugin, values);
	_collect_memory(plugin, values);
	_collect_network(plugin, values);
	*(unsigned *)plugin->spec = 1;

	if(++plugin->holding == plugin->capacity)
		*(unsigned *)plugin->spec = 0;

	char ts[20];
	snprintf(ts, 20, "%llu", plugin->next_run);
	json_object_object_add(plugin->values, ts, values);
}

/*
 * Network metrics
 *
 * This function extracts CPU metrics from /proc/net/dev.
 * The result is like below:
 *
 * eth0 283322939 303783 0 93889075 134312 0
 * lo 59971938 12500 0 59971938 12500 0
 *
 * (network name, bytes, packets, errors received/transmitted)
 */
void _collect_network(plugin_t *plugin, json_object *values) {
	FILE *pipe = popen("awk '{gsub(\":\",\"\",$1);print $1,$2,$3,$4,$10,$11,$12}' /proc/net/dev\
			| tail -n -2", "r");
	if(!pipe) return;

	char net_name[50];
	unsigned long long recv_byte, recv_pckt, recv_err;
	unsigned long long send_byte, send_pckt, send_err;

	unsigned long long total_recv_byte = 0;
	unsigned long long total_send_byte = 0;
	while(fscanf(pipe, "%s", net_name) == 1) {
		if(fscanf(pipe, "%llu%llu%llu%llu%llu%llu", &recv_byte, &recv_pckt, &recv_err, &send_byte, &send_pckt, &send_err) == 6) {
			if(!*(unsigned *)plugin->spec) {
				for(int i=0; i<sizeof(network_metric)/sizeof(char *); ++i) {
					char new_metric[100];
					snprintf(new_metric, 100, "%s|%s", network_metric[i], net_name);
					json_object_array_add(plugin->metric, json_object_new_string(new_metric));
				}
			}
			total_recv_byte += recv_byte;
			total_send_byte += send_byte;
			json_object_array_add(values, json_object_new_int64(recv_byte));
			json_object_array_add(values, json_object_new_int64(recv_pckt));
			json_object_array_add(values, json_object_new_int64(recv_err));
			json_object_array_add(values, json_object_new_int64(send_byte));
			json_object_array_add(values, json_object_new_int64(send_pckt));
			json_object_array_add(values, json_object_new_int64(send_err));
		} else break;
	}
	if(!*(unsigned *)plugin->spec) {
		json_object_array_add(plugin->metric, json_object_new_string("net_stat_total_recv_byte"));
		json_object_array_add(plugin->metric, json_object_new_string("net_stat_total_send_byte"));
	}
	json_object_array_add(values, json_object_new_int64(total_recv_byte));
	json_object_array_add(values, json_object_new_int64(total_send_byte));
	pclose(pipe);
}

/*
 * CPU metrics
 *
 * This function extracts CPU metrics from /proc/stat.
 * The result is like below:
 *
 * 0.000962047 0.000446021 0.998366
 *
 * (CPU usage of user, system, and idle)
 */
void _collect_cpu(plugin_t *plugin, json_object *values) {
	FILE *pipe;
	pipe = popen("awk '$1~/^cpu$/{tot=$2+$3+$4+$5;print $2/tot,$4/tot,$5/tot}' /proc/stat", "r");
	if(!pipe) return;

	double cpu_user, cpu_sys, cpu_idle;
	if(fscanf(pipe, "%lf%lf%lf", &cpu_user, &cpu_sys, &cpu_idle) != 3) {
		pclose(pipe);
		return;
	}
	if(!*(unsigned *)plugin->spec) {
		json_object_array_add(plugin->metric, json_object_new_string("cpu_stat_user"));
		json_object_array_add(plugin->metric, json_object_new_string("cpu_stat_sys"));
		json_object_array_add(plugin->metric, json_object_new_string("cpu_stat_idle"));
	}
	json_object_array_add(values, json_object_new_double(cpu_user));
	json_object_array_add(values, json_object_new_double(cpu_sys));
	json_object_array_add(values, json_object_new_double(cpu_idle));

	pclose(pipe);
}

/*
 * Disk metrics
 * This function extracts disk metrics.
 */
void _collect_disk(plugin_t *plugin, json_object *values) {
	FILE *pipe;
	pipe = popen("awk '$1~\"/dev/\"{gsub(\"/dev/\",\"\",$1);print $1,$2}' /proc/mounts", "r");
	if(!pipe) return;

	char part_name[10];
	char part_mount[30];
	unsigned long long io = 0;
	while(fscanf(pipe, "%s%s\n", part_name, part_mount) == 2) {
		/* Disk usage with df */
		char cmd[100];
		snprintf(cmd, 100, "df | awk '$1~\"%s\"{print $2,$3}'", part_name);
		FILE *subpipe = popen(cmd, "r");
		if(subpipe) {
			unsigned long long part_total, part_used;
			if(fscanf(subpipe, "%llu%llu", &part_total, &part_used) == 2) {
				if(!*(unsigned *)plugin->spec) {
					char metric_name[50];
					snprintf(metric_name, 50, "disk_stat_part_total|%s(%s)", part_name, part_mount);
					json_object_array_add(plugin->metric, json_object_new_string(metric_name));
					snprintf(metric_name, 50, "disk_stat_part_used|%s(%s)", part_name, part_mount);
					json_object_array_add(plugin->metric, json_object_new_string(metric_name));
					snprintf(metric_name, 50, "disk_stat_part_avail|%s(%s)", part_name, part_mount);
					json_object_array_add(plugin->metric, json_object_new_string(metric_name));
				}
				json_object_array_add(values, json_object_new_int64(part_total));
				json_object_array_add(values, json_object_new_int64(part_used));
				json_object_array_add(values, json_object_new_int64(part_total-part_used));
			}
			pclose(subpipe);
		}

		/* iostats */
		snprintf(cmd, 100, "lsblk -nro NAME,PHY-SeC | awk '$1~/^%s$/{print $2}'", part_name);
		subpipe = popen(cmd, "r");
		if(!subpipe) continue;
		unsigned int sector_size;
		if(fscanf(subpipe, "%u", &sector_size) != 1) {
			pclose(subpipe);
			continue;
		}
		pclose(subpipe);

		snprintf(cmd, 100, "awk '$3~/^%s$/{print $4,$6,$7,$8,$10,$11,$14}' /proc/diskstats", part_name);
		subpipe = popen(cmd, "r");
		if(subpipe) {
			unsigned long long r, rsec, rt, w, wsec, wt, weight;
			if(fscanf(subpipe, "%llu%llu%llu%llu%llu%llu%llu", &r, &rsec, &rt, &w, &wsec, &wt, &weight) == 7) {
				if(!*(unsigned *)plugin->spec) {
					for(int i=0; i<sizeof(disk_metric)/sizeof(char *); ++i) {
						char metric_name[50];
						snprintf(metric_name, 50, "%s|%s(%s)", disk_metric[i], part_name, part_mount);
						json_object_array_add(plugin->metric, json_object_new_string(metric_name));
					}
				}
				io += r+w;
				json_object_array_add(values, json_object_new_int64(r+w));
				json_object_array_add(values, json_object_new_int64(rt+wt));
				json_object_array_add(values, json_object_new_int64(r));
				json_object_array_add(values, json_object_new_double((double)rsec*(double)sector_size));
				json_object_array_add(values, json_object_new_int64(rt));
				json_object_array_add(values, json_object_new_int64(w));
				json_object_array_add(values, json_object_new_double((double)wsec*(double)sector_size));
				json_object_array_add(values, json_object_new_int64(wt));
				json_object_array_add(values, json_object_new_int64(weight));
			}
			pclose(subpipe);
		}
	}
	if(!*(unsigned *)plugin->spec) {
		json_object_array_add(plugin->metric, json_object_new_string("disk_stat_total"));
	}
	json_object_array_add(values, json_object_new_int64(io));
	pclose(pipe);
}

/*
 * Process metrics
 *
 * This function extracts top 5 processes in cpu(or memory) usage using ps command.
 * The result of the command is like below:
 *
 * gnome-shell      5.8
 * Xorg             0.9
 * gnome-settings-  0.8
 * gnome-session    0.6
 * ibus-x11         0.4
 *
 * (a command to execute the process, cpu(or memory) percentage that the process is using)
 */
void _collect_proc(plugin_t *plugin, json_object *values) {
	/* CPU */
	FILE *pipe = popen("ps -eo comm,pcpu --sort=-pcpu,-pmem --no-headers\
			| head -n 10", "r");
	if(!pipe) return;

	char name[100];
	double stat;
	for(int i=0; i<10; ++i) {
		if(fscanf(pipe, "%s%lf\n", name, &stat) == 2) {
			if(!*(unsigned *)plugin->spec) {
				char metric_name[50];
				snprintf(metric_name, 50, "proc_stat_cpu_top%d_name", i+1);
				json_object_array_add(plugin->metric, json_object_new_string(metric_name));
				snprintf(metric_name, 50, "proc_stat_cpu_top%d_cpu", i+1);
				json_object_array_add(plugin->metric, json_object_new_string(metric_name));
			}
			json_object_array_add(values, json_object_new_string(name));
			json_object_array_add(values, json_object_new_double(stat));
		}
	}
	pclose(pipe);

	/* Memory */
	pipe = popen("ps -eo comm,pmem --sort=-pmem,-pcpu --no-headers\
			| head -n 10", "r");
	if(!pipe) return;
	for(int i=0; i<10; ++i) {
		if(fscanf(pipe, "%s%lf\n", name, &stat) == 2) {
			if(!*(unsigned *)plugin->spec) {
				char metric_name[50];
				snprintf(metric_name, 50, "proc_stat_mem_top%d_name", i+1);
				json_object_array_add(plugin->metric, json_object_new_string(metric_name));
				snprintf(metric_name, 50, "proc_stat_mem_top%d_mem", i+1);
				json_object_array_add(plugin->metric, json_object_new_string(metric_name));
			}
			json_object_array_add(values, json_object_new_string(name));
			json_object_array_add(values, json_object_new_double(stat));
		}
	}
	pclose(pipe);

	pipe = popen("ps -eo user,comm,pcpu,pmem --no-headers\
			| awk '{c[$1\" \"$2]+=1;cpu[$1\" \"$2]+=$3;mem[$1\" \"$2]+=$4}\
			END{for(i in c)if(cpu[i]>0||mem[i]>0)print i,c[i],cpu[i],mem[i]}'\
			| sort -rk 4", "r");
	if(!pipe) return;
	char user[100], process[100];
	unsigned long count;
	double cpu, mem;
	int i=0;
	while(i++<5 && fscanf(pipe, "%s%s%lu%lf%lf", user, process, &count, &cpu, &mem) == 5) {
		if(!*(unsigned *)plugin->spec) {
			char metric_name[50];
			snprintf(metric_name, 50, "proc_stat_proc%d_user", i);
			json_object_array_add(plugin->metric, json_object_new_string(metric_name));
			snprintf(metric_name, 50, "proc_stat_proc%d_name", i);
			json_object_array_add(plugin->metric, json_object_new_string(metric_name));
			snprintf(metric_name, 50, "proc_stat_proc%d_count", i);
			json_object_array_add(plugin->metric, json_object_new_string(metric_name));
			snprintf(metric_name, 50, "proc_stat_proc%d_cpu", i);
			json_object_array_add(plugin->metric, json_object_new_string(metric_name));
			snprintf(metric_name, 50, "proc_stat_proc%d_mem", i);
			json_object_array_add(plugin->metric, json_object_new_string(metric_name));
		}
		json_object_array_add(values, json_object_new_string(user));
		json_object_array_add(values, json_object_new_string(process));
		json_object_array_add(values, json_object_new_int64(count));
		json_object_array_add(values, json_object_new_double(cpu));
		json_object_array_add(values, json_object_new_double(mem));
	}
	pclose(pipe);
}

/*
 * Memory metrics
 *
 * This function extracts memory metrics from /proc/meminfo.
 * The result is like below:
 *
 * 2048492
 * 88200
 * 1136352
 * 901300
 * 641872
 * 34359738367
 * 6880
 *
 * (total, free, cached, active, inactive, total virtual, and using virtual memory)
 */
void _collect_memory(plugin_t *plugin, json_object *values) {
	FILE *pipe = popen("awk '/^Mem[TF]|^Cached|Active:|Inactive:|Vmalloc[TU]/{print $2}' /proc/meminfo", "r");
	if(!pipe) return;

	unsigned long long total, free, cached, active, inactive, v_total, v_used;
	if(fscanf(pipe, "%llu%llu%llu%llu%llu%llu%llu", &total, &free, &cached, &active, &inactive, &v_total, &v_used) != 7) {
		pclose(pipe);
		return;
	}

	if(!*(unsigned *)plugin->spec) {
		json_object_array_add(plugin->metric, json_object_new_string("mem_stat_total"));
		json_object_array_add(plugin->metric, json_object_new_string("mem_stat_free"));
		json_object_array_add(plugin->metric, json_object_new_string("mem_stat_cached"));
		json_object_array_add(plugin->metric, json_object_new_string("mem_stat_user"));
		json_object_array_add(plugin->metric, json_object_new_string("mem_stat_sys"));
		json_object_array_add(plugin->metric, json_object_new_string("mem_stat_virtual_usage"));
	}
	json_object_array_add(values, json_object_new_int64(total));
	json_object_array_add(values, json_object_new_int64(free));
	json_object_array_add(values, json_object_new_int64(cached));
	json_object_array_add(values, json_object_new_int64((active+inactive)));
	json_object_array_add(values, json_object_new_int64((total-free-active-inactive)));
	json_object_array_add(values, json_object_new_double((double)v_used/(double)v_total));

	pclose(pipe);
}
