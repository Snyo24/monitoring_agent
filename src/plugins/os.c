/**
 * @file os.c
 * @author Snyo
 */
#include "plugins/os.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/statvfs.h>

#include "target.h"
#include "metadata.h"

#define OS_TICK 3
#define BFSZ 128
#define BPKB 1024

typedef target_t os_target_t;

int _collect_os_cpu(char *packet);
int _collect_os_disk(char *packet);
int _collect_os_proc(char *packet);
int _collect_os_memory(char *packet);
int _collect_os_network(char *packet);

int collect_os(void *_target, char *packet);

os_target_t os_target = {
    .tid = 0,
    .tip = aip,
    .type = "linux_linux_1.0",

    .tick = OS_TICK,
    
    .gather = collect_os,
    .fini = os_target_fini
};

void *os_target_init(int argc, char **argv) {
	return &os_target;
}

void os_target_fini(void *_target) {
	if(!_target)
		return;

    target_fini(_target);
}

int collect_os(void *_target, char *packet) {
    int n = 0;
    n += sprintf(packet+n, "\"cpu\":{");
	n += _collect_os_cpu(packet+n);
    n += sprintf(packet+n, "},\"disk\":{");
	n +=_collect_os_disk(packet+n);
    n += sprintf(packet+n, "},\"proc\":{");
	n += _collect_os_proc(packet+n);
    n += sprintf(packet+n, "},\"mem\":{");
	n += _collect_os_memory(packet+n);
    n += sprintf(packet+n, "},\"net\":{");
	n += _collect_os_network(packet+n);
    n += sprintf(packet+n, "}");

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
int _collect_os_cpu(char *packet) {
	FILE *pipe;
	pipe = popen("awk '$1~/^cpu$/{tot=$2+$3+$4+$5;print$2/tot,$4/tot,$5/tot}' /proc/stat", "r");
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
int _collect_os_disk(char *packet) {
	FILE *pipe;
	pipe = popen("awk '$1~\"^/dev/\"{print$1,$2}' /proc/mounts", "r");
	if(!pipe) return 0;

    struct {
        char name[BFSZ], mount[BFSZ];
        unsigned short num;
        unsigned long long tot, free, avail;
        unsigned long sec_size;
        unsigned long long r, rkb, rt, w, wkb, wt, weight;
    } dev[BFSZ];

	unsigned long long io_tot = 0;
    int k = 0;
	while(fscanf(pipe, "/dev/%128[^0-9]%hu%128s\n", dev[k].name, &dev[k].num, dev[k].mount) == 3) {
        // Usage
        struct statvfs vfs;
        if(statvfs(dev[k].mount, &vfs) < 0) continue;
        dev[k].tot   = vfs.f_blocks*vfs.f_frsize/BPKB;
        dev[k].free  = vfs.f_bfree *vfs.f_bsize /BPKB;
        dev[k].avail = vfs.f_bavail*vfs.f_bsize /BPKB;
        // !Usage

        // Sector size
        char cmd[BFSZ];
        snprintf(cmd, BFSZ, "/sys/block/%s/queue/logical_block_size", dev[k].name);
        FILE *subpipe = fopen(cmd, "r");
        if(!subpipe) continue;
        unsigned long sec_size;
        int success = fscanf(subpipe, "%lu", &sec_size)==1;
        fclose(subpipe);
        if(!success) continue;
        // !Sector size

        // Diskstat
		snprintf(cmd, BFSZ, "awk '$3~\"%s%hu\"{print $4,$6,$7,$8,$10,$11,$14}' /proc/diskstats", dev[k].name, dev[k].num);
		subpipe = popen(cmd, "r");
        if(!subpipe) continue;
        if(fscanf(subpipe, "%llu%llu%llu%llu%llu%llu%llu", &dev[k].r, &dev[k].rkb, &dev[k].rt, &dev[k].w, &dev[k].wkb, &dev[k].wt, &dev[k].weight) == 7) {
            dev[k].rkb = dev[k].rkb * sec_size / BPKB;
            dev[k].wkb = dev[k].wkb * sec_size / BPKB;
            io_tot += dev[k].r+dev[k].w;
        }
        pclose(subpipe);
        // !Diskstat

        k++;
	}
	pclose(pipe);

    int n = 0;
    n += sprintf(packet+n, "\"name\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s\"%s%hu(%s)\"", i?",":"", dev[i].name, dev[i].num, dev[i].mount);
    n += sprintf(packet+n, "],\"tot\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%llu", i?",":"", dev[i].tot);
    n += sprintf(packet+n, "],\"free\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%llu", i?",":"", dev[i].free);
    n += sprintf(packet+n, "],\"avail\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%llu", i?",":"", dev[i].avail);
    n += sprintf(packet+n, "],\"r\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%llu", i?",":"", dev[i].r);
    n += sprintf(packet+n, "],\"rsec\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%llu", i?",":"", dev[i].rkb);
    n += sprintf(packet+n, "],\"rt\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%llu", i?",":"", dev[i].rt);
    n += sprintf(packet+n, "],\"w\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%llu", i?",":"", dev[i].w);
    n += sprintf(packet+n, "],\"wsec\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%llu", i?",":"", dev[i].wkb);
    n += sprintf(packet+n, "],\"wt\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%llu", i?",":"", dev[i].wt);
    n += sprintf(packet+n, "],\"weight\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%llu", i?",":"", dev[i].weight);
    n += sprintf(packet+n, "],\"io_tot\":%llu", io_tot);

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
int _collect_os_proc(char *packet) {
    struct {
        char name[BFSZ];
        char user[BFSZ];
        unsigned long count;
        float cpu;
        float mem;
    } proc[10];

    int n = 0;

    // CPU
	FILE *pipe = popen("ps -eo comm,pcpu --no-headers | awk '{c[$1]+=1;cpu[$1]+=$2} END{for(i in c)if(cpu[i]>0)print i,cpu[i]}' | sort -rk2,2 | head -n 10", "r");
	if(!pipe) return 0;

    n += sprintf(packet+n, "\"cpu_top10\":{");
    int k = 0;
	while(fscanf(pipe, "%128s%f\n", proc[k].name, &proc[k].cpu) == 2) k++;
	pclose(pipe);

    n += sprintf(packet+n, "\"name\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s\"%s\"", i?",":"", proc[i].name);
    n += sprintf(packet+n, "],\"cpu\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%.1f", i?",":"", proc[i].cpu);
    n += sprintf(packet+n, "]}");

    // !CPU

    // MEMORY
	pipe = popen("ps -eo comm,pmem --no-headers | awk '{c[$1]+=1;mem[$1]+=$2} END{for(i in c)if(mem[i]>0)print i,mem[i]}' | sort -rk2,2 | head -n 10", "r");
	if(!pipe) return n;

    n += sprintf(packet+n, ",\"mem_top10\":{");
    k = 0;
	while(fscanf(pipe, "%128s%f\n", proc[k].name, &proc[k].mem) == 2) k++;
	pclose(pipe);

    n += sprintf(packet+n, "\"name\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s\"%s\"", i?",":"", proc[i].name);
    n += sprintf(packet+n, "],\"cpu\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%.1f", i?",":"", proc[i].mem);
    n += sprintf(packet+n, "]}");
    // !MEMORY

    // PROCESSES
	pipe = popen("ps -eo comm,user,pcpu,pmem --no-headers | awk '{c[$1\" \"$2]+=1;cpu[$1\" \"$2]+=$3;mem[$1\" \"$2]+=$4} END{for(i in c)if(cpu[i]>0||mem[i]>0)print i,c[i],cpu[i],mem[i]}' | sort -r -k5,5 -k4,4 | head -n 10", "r");
	if(!pipe) return n;

    n += sprintf(packet+n, "},\"list\":{");
    k = 0;
	while(fscanf(pipe, "%128s%128s%lu%f%f", proc[k].name, proc[k].user, &proc[k].count, &proc[k].cpu, &proc[k].mem) == 5) k++;
    n += sprintf(packet+n, "\"name\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s\"%s\"", i?",":"", proc[i].name);
    n += sprintf(packet+n, "],\"user\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s\"%s\"", i?",":"", proc[i].user);
    n += sprintf(packet+n, "],\"count\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%lu", i?",":"", proc[i].count);
    n += sprintf(packet+n, "],\"cpu\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%.1f", i?",":"", proc[i].cpu);
    n += sprintf(packet+n, "],\"mem\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%.1f", i?",":"", proc[i].mem);
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
int _collect_os_memory(char *packet) {
	FILE *pipe = popen("awk '/^Mem[TF]|^Cached|Active:|Inactive:|Vmalloc[TU]/{print$2}' /proc/meminfo", "r");
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
int _collect_os_network(char *packet) {
	FILE *pipe = popen("awk '{print$1,$2,$3,$4,$10,$11,$12}' /proc/net/dev | tail -n +3", "r");
	if(!pipe) return 0;

    int n = 0;

    int k = 0;
    struct {
        char name[BFSZ];
        unsigned long long i_byte, i_pckt, i_err;
        unsigned long long o_byte, o_pckt, o_err;
    } net_if[BFSZ];
    unsigned long long i_tot = 0, o_tot = 0;

	while(fscanf(pipe, "%128[^:]:", net_if[k].name) == 1) {
		if(fscanf(pipe, "%llu%llu%llu%llu%llu%llu\n", &net_if[k].i_byte, &net_if[k].i_pckt, &net_if[k].i_err, &net_if[k].o_byte, &net_if[k].o_pckt, &net_if[k].o_err) == 6) {
            i_tot += net_if[k].i_byte;
			o_tot += net_if[k].o_byte;
            k++;
		} else break;
	}
	pclose(pipe);

    n += sprintf(packet+n, "\"name\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s\"%s\"", i?",":"", net_if[i].name);
    n += sprintf(packet+n, "],\"i_byte\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%llu", i?",":"", net_if[i].i_byte);
    n += sprintf(packet+n, "],\"o_byte\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%llu", i?",":"", net_if[i].o_byte);
    n += sprintf(packet+n, "],\"i_pckt\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%llu", i?",":"", net_if[i].i_pckt);
    n += sprintf(packet+n, "],\"o_pckt\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%llu", i?",":"", net_if[i].o_pckt);
    n += sprintf(packet+n, "],\"i_err\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%llu", i?",":"", net_if[i].i_err);
    n += sprintf(packet+n, "],\"o_err\":[");
    for(int i=0; i<k; i++)
        n += sprintf(packet+n, "%s%llu", i?",":"", net_if[i].o_err);
    n += sprintf(packet+n, "],\"i_tot\":%llu,\"o_tot\":%llu", i_tot, o_tot);

    return n;
}
