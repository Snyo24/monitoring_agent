/**
 * @file plugin.c
 * @author Snyo
 */
#include "plugin.h"

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#include <zlog.h>

#include "metadata.h"
#include "packet.h"
#include "sender.h"
#include "util.h"

int plugin_fini(void *_p);
int plugin_exec(void *_p);
int plugin_reg(void *_p);
int plugin_gather(void *_p);

int plugin_init(void *_p) {
    if(!_p) return -1;
    memset(_p, 0, sizeof(plugin_t));

	if(runnable_init(_p) < 0)
		return -1;

    plugin_t *p = _p;
    p->routine = plugin_exec;

    return 0;
}

int plugin_fini(void *_p) {
    if(!_p) return -1;

    plugin_t *p = _p;
    DEBUG(zlog_debug(p->tag, "Finialize"));

    if(p->fini) p->fini(p);
    for(packet_t *pckt=p->packets; pckt; free_packet(p->packets));

    return 0;
}

int plugin_exec(void *_p) {
    if(!_p) return -1;

    plugin_t *p = _p;
    DEBUG(if(!p->tag) p->tag = zlog_get_category(p->type));
    DEBUG(if(!p->tag));
    DEBUG(zlog_debug(p->tag, "Execute"));

    if(p->exec)
        if(p->exec(_p) < 0)
            return -1;
    p->routine = plugin_reg;

    return 0;
}

int plugin_reg(void *_p) {
    if(!_p) return -1;

    plugin_t *p = _p;
    DEBUG(zlog_debug(p->tag, "Register"));

    // TODO
    p->tid = epoch_time()*epoch_time()*epoch_time();

    char reg_str[1000];
    snprintf(reg_str, 1000, "{\n\
            \"os\":\"%s\",\n\
            \"hostname\":\"%s\",\n\
            \"license\":\"%s\",\n\
            \"aid\":\"%s\",\n\
            \"tid\":%llu,\n\
            \"ip\":\"%s\",\n\
            \"agent_type\":\"%s\",\n\
            \"target_type\":\"%s\"\n\
            }",\
            os, host, license, aid, p->tid, aip, type, p->type);
    extern sender_t sndr;
    if(sender_post(&sndr, reg_str, REGISTER) < 0)
        return -1;

    p->routine = plugin_gather;

    return 0;
}

int plugin_gather(void *_p) {
    plugin_t *p = (plugin_t *)_p;

    packet_t *pckt = p->writing;
    if(!p->writing) {
        pckt = alloc_packet();
        p->writing = pckt;
        p->packets = pckt;
        pckt->ofs += sprintf(pckt->payload+pckt->ofs, "{\"license\":\"%s\",\"tid\":%llu,\"metrics\":[", license, p->tid);
    }
    if(packet_expire(pckt)) {
        pckt->ofs += sprintf(pckt->payload+pckt->ofs, "]}");
        packet_t *old = pckt;
        pckt->next = alloc_packet();
        p->writing = pckt->next;
        pckt = pckt->next;
        pckt->ofs += sprintf(pckt->payload+pckt->ofs, "{\"license\":\"%s\",\"tid\":%llu,\"metrics\":[", license, p->tid);
        old->ready = 1;
    }

    DEBUG(zlog_debug(p->tag, ".. Gather"));

    epoch_t begin = epoch_time();

    pckt->ofs += sprintf(pckt->payload+pckt->ofs, "%s{\"timestamp\":%llu,\"values\":{", packet_new(pckt)?"":",", begin);
    pckt->ofs += p->gather(_p, pckt->payload+pckt->ofs);
    pckt->ofs += sprintf(pckt->payload+pckt->ofs, "}}");

    DEBUG(zlog_debug(p->tag, ".. %d bytes, in %llums", pckt->ofs, epoch_time()-begin));

    return pckt->ofs;
}
