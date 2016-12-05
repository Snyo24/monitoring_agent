/**
 * @file os.c
 * @author Snyo
 */
#include "plugins/os.h"
#include "plugin.h"

#include <stdio.h>

#include "metadata.h"

#define OS_PLUGIN_TICK 3
#define OS_PLUGIN_CAPACITY 20

typedef struct os_plugin_t {
	plugin_t;
} os_plugin_t;

typedef struct os_target_t {
    char host[50];
} os_target_t;

int _collect_cpu(char *packet);
int _collect_disk(char *packet);
int _collect_proc(char *packet);
int _collect_memory(char *packet);
int _collect_network(char *packet);

int collect_os(void *_target, char *packet);

void *os_plugin_init(int argc, char **argv) {
    os_plugin_t *plugin = malloc(sizeof(os_plugin_t));
    if(!plugin) return NULL;

	os_target_t *target = malloc(sizeof(os_target_t));
	if(!target) return NULL;

	snprintf(target->host, 50, "%s", ip);
    plugin->tgv[0]  = target;
    plugin->tgc     = 1;

	plugin->type    = "linux_linux_1.0";
	plugin->period  = OS_PLUGIN_TICK;

	plugin->collect = collect_os;
	plugin->fini    = os_plugin_fini;

	return plugin;
}

void os_plugin_fini(void *_plugin) {
	if(!_plugin)
		return;

    plugin_fini(_plugin);

    os_plugin_t *plugin = _plugin;
	if(plugin->tgv[0])
		free(plugin->tgv);
    free(plugin);
}

int collect_os(void *_target, char *packet) {
    int n = 0;
    n += sprintf(packet+n, "{");
    n += sprintf(packet+n, "\"cpu\":{");
	n += _collect_cpu(packet+n);
    n += sprintf(packet+n, "},\"disk\":{");
	n +=_collect_disk(packet+n);
    n += sprintf(packet+n, "},\"proc\":{");
	n += _collect_proc(packet+n);
    n += sprintf(packet+n, "},\"mem\":{");
	n += _collect_memory(packet+n);
    n += sprintf(packet+n, "},\"net\":{");
	n += _collect_network(packet+n);
    n += sprintf(packet+n, "}}");

    return n;
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
int _collect_cpu(char *packet) {
	FILE *pipe;
	pipe = popen("awk '$1~/^cpu$/{tot=$2+$3+$4+$5;print $2/tot,$4/tot,$5/tot}' /proc/stat", "r");
	if(!pipe) return 0;

    int n = 0;
	float cpu_user, cpu_sys, cpu_idle;
	if(fscanf(pipe, "%f%f%f", &cpu_user, &cpu_sys, &cpu_idle) == 3)
        n += sprintf(packet+n, "\"user\":%.2f,\"sys\":%.2f,\"idle\":%.2f", cpu_user, cpu_sys, cpu_idle);

	pclose(pipe);

    return n;
}

/*
 * Disk metrics
 * This function extracts disk metrics.
 */
int _collect_disk(char *packet) {
	FILE *pipe;
	pipe = popen("awk '$1~\"^/dev/\"{print $1,$2}' /proc/mounts", "r");
	if(!pipe) return 0;

    int n = 0;
    n += sprintf(packet+n, "\"list\":{");

	char part_name[9];
    unsigned short part_num;
	char part_mount[49];
	unsigned long long io_tot = 0;
    int sw = 0;
	while(fscanf(pipe, "/dev/%[^0-9]%hu%49s\n", part_name, &part_num, part_mount) == 3) {
        int n2 = 0;
        n2 += sprintf(packet+n+n2, "%s\"%s%hu(%s)\":{", sw?",":"", part_name, part_num, part_mount);
        sw = 1;

        // Usage
		char cmd[99];
		snprintf(cmd, 99, "df | awk '$1~\"%s%hu\"{print $2,$3}'", part_name, part_num);
		FILE *subpipe = popen(cmd, "r");
        if(!subpipe) continue;
        unsigned long long part_total, part_used;
        if(fscanf(subpipe, "%llu%llu", &part_total, &part_used) == 2)
            n2 += sprintf(packet+n+n2, "\"tot\":%llu,\"used\":%llu,\"avail\":%llu", part_total, part_used, part_total-part_used);
        pclose(subpipe);
        // !Usage

        // Block size
		snprintf(cmd, 99, "cat /sys/block/%s/queue/logical_block_size", part_name);
		subpipe = popen(cmd, "r");
		if(!subpipe) continue;
		unsigned long sector;
		if(fscanf(subpipe, "%lu", &sector) != 1)
            sector = 0;
		pclose(subpipe);
        // !Block size

        // Diskstat
		snprintf(cmd, 99, "awk '$3~/^%s%hu$/{print $4,$6,$7,$8,$10,$11,$14}' /proc/diskstats", part_name, part_num);
		subpipe = popen(cmd, "r");
        if(!subpipe) continue;
        unsigned long long r, rsec, rt, w, wsec, wt, weight;
        if(fscanf(subpipe, "%llu%llu%llu%llu%llu%llu%llu", &r, &rsec, &rt, &w, &wsec, &wt, &weight) == 7) {
            n2 += sprintf(packet+n+n2, ",\"io\":%llu,\"iot\":%llu,\"r\":%llu,\"rsec\":%llu,\"rt\":%llu,\"w\":%llu,\"wsec\":%llu,\"wt\":%llu,\"weight\":%llu", r+w, rt+wt, r, rsec*sector/1024, rt, w, wsec*sector/1024, wt, weight);
            io_tot += r+w;
        }
        pclose(subpipe);
        // !Diskstat

        n2 += sprintf(packet+n+n2, "}");
        n += n2;
	}
    n += sprintf(packet+n, ",\"io_tot\":%llu}", io_tot);
	pclose(pipe);

    return n;
}

/*
 * Process metrics
 *
 * This function extracts top 10 processes in cpu(or memory) usage using ps command.
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
int _collect_proc(char *packet) {
    // CPU
	FILE *pipe = popen("ps -eo comm,pcpu --no-headers | awk '{c[$1]+=1;cpu[$1]+=$2} END{for(i in c)if(cpu[i]>0)print i,cpu[i]}' | sort -rk2,2 | head -n 10", "r");
	if(!pipe) return 0;

    int n = 0;

    n += sprintf(packet+n, "\"cpu_top10\":{");
    int sw = 0;
	char proc[99];
	float cpu;
	while(fscanf(pipe, "%s%f\n", proc, &cpu) == 2) {
        n += sprintf(packet+n, "%s\"%s\":%.2f", sw?",":"", proc, cpu);
        sw = 1;
    }
	pclose(pipe);
    // !CPU

    // MEMORY
	pipe = popen("ps -eo comm,pmem --no-headers | awk '{c[$1]+=1;mem[$1]+=$2} END{for(i in c)if(mem[i]>0)print i,mem[i]}' | sort -rk2,2 | head -n 10", "r");
	if(!pipe) return n;

    n += sprintf(packet+n, "},\"mem_top10\":{");
    sw = 0;
    float mem;
	while(fscanf(pipe, "%s%f\n", proc, &mem) == 2) {
        n += sprintf(packet+n, "%s\"%s\":%.2f", sw?",":"", proc, mem);
        sw = 1;
    }
	pclose(pipe);
    // !MEMORY

    // PROCESSES
	pipe = popen("ps -eo comm,user,pcpu,pmem --no-headers | awk '{c[$1\" \"$2]+=1;cpu[$1\" \"$2]+=$3;mem[$1\" \"$2]+=$4} END{for(i in c)if(cpu[i]>0||mem[i]>0)print i,c[i],cpu[i],mem[i]}' | sort -r -k5,5 -k4,4 | head -n 10", "r");
	if(!pipe) return n;

    n += sprintf(packet+n, "},\"list\":[");
    sw = 0;
	char user[99];
	unsigned long count;
	while(fscanf(pipe, "%99s%99s%lu%f%f", proc, user, &count, &cpu, &mem) == 5) {
        n += sprintf(packet+n, "%s[\"%s\",\"%s\",%lu,%.2f,%.2f]", sw?",":"", proc, user, count, cpu, mem);
        sw = 1;
	}
    n += sprintf(packet+n, "]");
	pclose(pipe);
    // !PROCESSES

    return n;
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
int _collect_memory(char *packet) {
	FILE *pipe = popen("awk '/^Mem[TF]|^Cached|Active:|Inactive:|Vmalloc[TU]/{print $2}' /proc/meminfo", "r");
	if(!pipe) return 0;

    int n = 0;

	unsigned long long total, free, cached, active, inactive, v_total, v_used;
	if(fscanf(pipe, "%llu%llu%llu%llu%llu%llu%llu", &total, &free, &cached, &active, &inactive, &v_total, &v_used) == 7)
        n += sprintf(packet+n, "\"total\":%llu,\"free\":%llu,\"cached\":%llu,\"user\":%llu,\"sys\":%llu,\"virtual\":%.2lf", total, free, cached, active+inactive, total-free-active-inactive, (double)v_used/(double)v_total);
	pclose(pipe);
    
    return n;
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
int _collect_network(char *packet) {
	FILE *pipe = popen("awk '{print $1,$2,$3,$4,$10,$11,$12}' /proc/net/dev | tail -n +3", "r");
	if(!pipe) return 0;

    int n = 0;
    n += sprintf(packet+n, "\"list\":{");

    int sw = 0;
	char net[50];
	unsigned long long i_byte, i_pckt, i_err, i_tot = 0;
	unsigned long long o_byte, o_pckt, o_err, o_tot = 0;
	while(fscanf(pipe, "%[^:]:", net) == 1) {
		if(fscanf(pipe, "%llu%llu%llu%llu%llu%llu\n", &i_byte, &i_pckt, &i_err, &o_byte, &o_pckt, &o_err) == 6) {
            n += sprintf(packet+n, "%s\"%s\":{\"i_byte\":%llu,\"i_pckt\":%llu,\"i_err\":%llu,\"o_byte\":%llu,\"o_pckt\":%llu,\"o_err\":%llu}", sw?",":"", net, i_byte, i_pckt, i_err, o_byte, o_pckt, o_err);

            sw = 1;
            i_tot += i_byte;
			o_tot += o_byte;
		} else break;
	}
    n += sprintf(packet+n, "},\"i_tot\":%llu,\"o_tot\":%llu", i_tot, o_tot);
	pclose(pipe);

    return n;
}
