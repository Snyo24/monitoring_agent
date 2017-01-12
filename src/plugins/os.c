/**
 * @file os.c
 * @author Snyo
 */
#include "plugins/os.h"
#include "plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/statvfs.h>

#include <zlog.h>

#include "packet.h"
#include "util.h"

#define OS_TICK 2.977F

int os_module_cmp(void *_m1, void *_m2, int size);
int os_gather(void *_p, packet_t *pkt);

int _os_gather_cpu(void *_m, packet_t *pkt);
int _os_gather_disk(void *_m, packet_t *pkt);
int _os_gather_proc(void *_m, packet_t *pkt);
int _os_gather_memory(void *_m, packet_t *pkt);
int _os_gather_network(void *_m, packet_t *pkt);

int load_os_module(plugin_t *p, int argc, char **argv) {
    if(!p) return -1;

    p->tick = OS_TICK;
    p->tip  = NULL;
    p->type = "linux_ubuntu_1.0";

    p->prep = NULL;
    p->fini = NULL;
    p->gather = os_gather;
    p->cmp = os_module_cmp;

    p->module_size = 0;
    p->module = 0;

	return 0;
}

int os_module_cmp(void *_m1, void *_m2, int size) {
    return 0;
}

int os_gather(void *_m, packet_t *pkt) {
    return packet_gather(pkt, "cpu",  _os_gather_cpu, _m)
        & packet_gather(pkt, "disk", _os_gather_disk, _m)
        & packet_gather(pkt, "proc", _os_gather_proc, _m)
        & packet_gather(pkt, "mem",  _os_gather_memory, _m)
        & packet_gather(pkt, "net",  _os_gather_network, _m);
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
int _os_gather_cpu(void *_m, packet_t *pkt) {
	FILE *pipe;
	pipe = popen("awk '$1~/^cpu$/{tot=$2+$3+$4+$5;print$2/tot,$4/tot,$5/tot}' /proc/stat", "r");
	if(!pipe) return ENODATA;

	float cpu_user, cpu_sys, cpu_idle;
	if(fscanf(pipe, "%f%f%f", &cpu_user, &cpu_sys, &cpu_idle) == 3)
        packet_append(pkt, "\"user\":%.2f,\"sys\":%.2f,\"idle\":%.2f", cpu_user, cpu_sys, cpu_idle);
	pclose(pipe);

    return ENONE;
}

/*
 * Disk metrics
 * This function extracts disk metrics.
 */
int _os_gather_disk(void *_m, packet_t *pkt) {
    int error = ENODATA;
	FILE *pipe;
	pipe = popen("awk '$1~\"^/dev/\"{print$1,$2}' /proc/mounts", "r");
	if(!pipe) return error;

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

    if(k == 0) return error;

    packet_append(pkt, "\"name\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s\"%s%hu(%s)\"", i?",":"", dev[i].name, dev[i].num, dev[i].mount);
    packet_append(pkt, "],\"tot\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%llu", i?",":"", dev[i].tot);
    packet_append(pkt, "],\"free\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%llu", i?",":"", dev[i].free);
    packet_append(pkt, "],\"avail\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%llu", i?",":"", dev[i].avail);
    packet_append(pkt, "],\"r\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%llu", i?",":"", dev[i].r);
    packet_append(pkt, "],\"rsec\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%llu", i?",":"", dev[i].rkb);
    packet_append(pkt, "],\"rt\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%llu", i?",":"", dev[i].rt);
    packet_append(pkt, "],\"w\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%llu", i?",":"", dev[i].w);
    packet_append(pkt, "],\"wsec\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%llu", i?",":"", dev[i].wkb);
    packet_append(pkt, "],\"wt\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%llu", i?",":"", dev[i].wt);
    packet_append(pkt, "],\"weight\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%llu", i?",":"", dev[i].weight);
    packet_append(pkt, "],\"io_tot\":%llu", io_tot);

    return ENONE;
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
int _os_gather_proc(void *_m, packet_t *pkt) {
    int error = ENODATA;

    struct {
        char name[BFSZ];
        char user[BFSZ];
        unsigned long count;
        float cpu;
        float mem;
    } proc[10];

    // CPU
	FILE *pipe = popen("ps -eo comm,pcpu --no-headers | awk '{c[$1]+=1;cpu[$1]+=$2} END{for(i in c)if(cpu[i]>0)print i,cpu[i]}' | sort -grk2,2 | head -n 10", "r");
	if(!pipe) return error;

    int k = 0;
	while(fscanf(pipe, "%128s%f\n", proc[k].name, &proc[k].cpu) == 2) k++;
	pclose(pipe);

    if(k > 0) {
        error = ENONE;
        packet_append(pkt, "\"cpu_top10\":{\"name\":[");
        for(int i=0; i<k; i++)
            packet_append(pkt, "%s\"%s\"", i?",":"", proc[i].name);
        packet_append(pkt, "],\"cpu\":[");
        for(int i=0; i<k; i++)
            packet_append(pkt, "%s%.1f", i?",":"", proc[i].cpu);
        packet_append(pkt, "]}");
    }
    // !CPU

    // MEMORY
	pipe = popen("ps -eo comm,pmem --no-headers | awk '{c[$1]+=1;mem[$1]+=$2} END{for(i in c)if(mem[i]>0)print i,mem[i]}' | sort -grk2,2 | head -n 10", "r");
	if(!pipe) return error;

    k = 0;
	while(fscanf(pipe, "%128s%f\n", proc[k].name, &proc[k].mem) == 2) k++;
	pclose(pipe);

    if(k > 0) {
        error = ENONE;
        packet_append(pkt, "%s\"mem_top10\":{\"name\":[", pkt->payload[pkt->size-1]=='{'?"":",");
        for(int i=0; i<k; i++)
            packet_append(pkt, "%s\"%s\"", i?",":"", proc[i].name);
        packet_append(pkt, "],\"mem\":[");
        for(int i=0; i<k; i++)
            packet_append(pkt, "%s%.1f", i?",":"", proc[i].mem);
        packet_append(pkt, "]}");
    }
    // !MEMORY

    // PROCESSES
	pipe = popen("ps -eo comm,user,pcpu,pmem --no-headers | awk '{c[$1\" \"$2]+=1;cpu[$1\" \"$2]+=$3;mem[$1\" \"$2]+=$4} END{for(i in c)if(cpu[i]>0||mem[i]>0)print i,c[i],cpu[i],mem[i]}' | sort -gr -k5,5 -k4,4 | head -n 10", "r");
	if(!pipe) return error;

    k = 0;
	while(fscanf(pipe, "%128s%128s%lu%f%f", proc[k].name, proc[k].user, &proc[k].count, &proc[k].cpu, &proc[k].mem) == 5) k++;
	pclose(pipe);

    if(k > 0) {
        error = ENONE;
        packet_append(pkt, "%s\"list\":{\"name\":[", pkt->payload[pkt->size-1]=='{'?"":",");
        for(int i=0; i<k; i++)
            packet_append(pkt, "%s\"%s\"", i?",":"", proc[i].name);
        packet_append(pkt, "],\"user\":[");
        for(int i=0; i<k; i++)
            packet_append(pkt, "%s\"%s\"", i?",":"", proc[i].user);
        packet_append(pkt, "],\"count\":[");
        for(int i=0; i<k; i++)
            packet_append(pkt, "%s%lu", i?",":"", proc[i].count);
        packet_append(pkt, "],\"cpu\":[");
        for(int i=0; i<k; i++)
            packet_append(pkt, "%s%.1f", i?",":"", proc[i].cpu);
        packet_append(pkt, "],\"mem\":[");
        for(int i=0; i<k; i++)
            packet_append(pkt, "%s%.1f", i?",":"", proc[i].mem);
        packet_append(pkt, "]}");
    }
    // !PROCESSES
    return error;
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
int _os_gather_memory(void *_m, packet_t *pkt) {
    int error = ENODATA;
	FILE *pipe = popen("awk '/^Mem[TF]|^Cached|Active:|Inactive:|Vmalloc[TU]/{print$2}' /proc/meminfo", "r");
	if(!pipe) return error;

	unsigned long long total, free, cached, active, inactive, v_total, v_used;
	if(fscanf(pipe, "%llu%llu%llu%llu%llu%llu%llu", &total, &free, &cached, &active, &inactive, &v_total, &v_used) == 7) {
        error = ENONE;
        packet_append(pkt, "\"total\":%llu,\"free\":%llu,\"cached\":%llu,\"user\":%llu,\"sys\":%llu,\"virtual\":%.2lf", total, free, cached, active+inactive, total-free-active-inactive, (double)v_used/(double)v_total);
    }
	pclose(pipe);

    return error;
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
int _os_gather_network(void *_m, packet_t *pkt) {
    int error = ENODATA;

	FILE *pipe = popen("awk '{print$1,$2,$3,$4,$10,$11,$12}' /proc/net/dev | tail -n +3", "r");
	if(!pipe) return error;

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

    if(k == 0) return error;

    packet_append(pkt, "\"name\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s\"%s\"", i?",":"", net_if[i].name);
    packet_append(pkt, "],\"i_byte\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%llu", i?",":"", net_if[i].i_byte);
    packet_append(pkt, "],\"o_byte\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%llu", i?",":"", net_if[i].o_byte);
    packet_append(pkt, "],\"i_pckt\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%llu", i?",":"", net_if[i].i_pckt);
    packet_append(pkt, "],\"o_pckt\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%llu", i?",":"", net_if[i].o_pckt);
    packet_append(pkt, "],\"i_err\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%llu", i?",":"", net_if[i].i_err);
    packet_append(pkt, "],\"o_err\":[");
    for(int i=0; i<k; i++)
        packet_append(pkt, "%s%llu", i?",":"", net_if[i].o_err);
    packet_append(pkt, "],\"i_tot\":%llu,\"o_tot\":%llu", i_tot, o_tot);

    return ENONE;
}
