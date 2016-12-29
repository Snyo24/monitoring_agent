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

int plugin_fini(plugin_t *p);
int plugin_prep(plugin_t *p);
int plugin_regr(plugin_t *p);
int plugin_gather(plugin_t *p);

int plugin_init(plugin_t *p) {
    if(!p) return -1;

	if(runnable_init(&p->r) < 0)
		return -1;

    p->packets = 0;
    p->working = 0;
    p->oob = 0;

    runnable_change_task(&p->r, plugin_prep);

    return 0;
}

int plugin_fini(plugin_t *p) {
    if(!p) return -1;

    DEBUG(zlog_debug(p->tag, "Finialize"));

    if(p->fini) p->fini(p);
    packet_t *prev, *curr = p->packets;
    while(curr) {
        prev = curr;
        curr = curr->next;
        packet_free(prev);
    }

    return 0;
}

int plugin_prep(plugin_t *p) {
    if(!p) return -1;

    DEBUG(if(!p->tag) p->tag = zlog_get_category(p->type));
    DEBUG(if(!p->tag));

    DEBUG(zlog_debug(p->tag, ".. Prepare"));

    if(p->prep)
        if(p->prep(p->module) < 0)
            return -1;

    runnable_change_task(&p->r, plugin_regr);

    return plugin_regr(p);
}

int plugin_regr(plugin_t *p) {
    if(!p) return -1;

    DEBUG(zlog_debug(p->tag, ".. Register"));

    if(!p->oob) {
        p->tid = epoch_time()*epoch_time()*epoch_time();
        p->oob = packet_alloc(REGISTER);
        p->oob->state = READY;
        packet_append(p->oob, "{\"license\":\"%s\",\"aid\":%llu,\"tid\":%20llu,\"agent_type\":\"%s\",\"target_type\":\"%s\",\"os\":\"%s\",\"hostname\":\"%s\",\"ip\":\"%s\"%s%s%s}", license, aid, p->tid, type, p->type, os, host, aip, p->tip?",\"target_ip\":\"":"", p->tip?p->tip:"", p->tip?"\"":"");
    } else {
        sprintf(p->oob->payload+55, "%20llu", p->tid);
        p->oob->payload[75] = ',';
    }

    if(post(p->oob) < 0) {
        switch(p->oob->error) {
            case ETARGETREG:
            p->tid = epoch_time()*epoch_time()*epoch_time();
            break;

            case ETARGETPCH:
            break;

            case EAGENTREG:
            break;

            case EAGENTPCH:
            break;
        }
        return -1;
    }

    packet_free(p->oob);
    p->oob = 0;

    p->working = packet_alloc(METRIC);
    p->packets = p->working;
    runnable_change_task(&p->r, plugin_gather);

    return 0;
}

int plugin_alert(plugin_t *p, const char *stat) {
    if(!p->oob) {
        p->oob = packet_alloc(ALERT);
        p->oob->state = READY;
        packet_append(p->oob, "{\"license\":\"%s\",\"tid\":%llu,\"status\":\"%s\"}", license, p->tid, stat);
    }
    if(post(p->oob) < 0)
        return -1;

    packet_free(p->oob);
    p->oob = 0;
    runnable_change_task(&p->r, plugin_gather);

    return 0;
}

int plugin_alert_up(plugin_t *p) {
    return plugin_alert(p, "up");
}

int plugin_alert_down(plugin_t *p) {
    return plugin_alert(p, "down");
}

int plugin_gather(plugin_t *p) {
    while(p->packets->state==DONE) {
        packet_t *pkt = p->packets;
        p->packets = p->packets->next;
        free(pkt);
    }

    DEBUG(zlog_debug(p->tag, ".. Gather"));

    packet_t *pkt = p->working;

    if(pkt->state == EMPTY) {
        packet_append(pkt, "{\"license\":\"%s\",\"tid\":%llu,\"metrics\":[", license, p->tid);
        pkt->state = BEGIN;
    }

    epoch_t begin = epoch_time();
    if(pkt->state == BEGIN)
        pkt->started = begin;

    packet_transaction(pkt);
    if(pkt->state == WROTE)
        packet_append(pkt, ",");

    packet_append(pkt, "{\"timestamp\":%llu,", begin);
    switch(packet_gather(pkt, "values", p->gather, p->module)) {
        case ENONE:
        packet_append(pkt, "}");
        if(pkt->state == BEGIN)
            pkt->state = WROTE;
        packet_commit(pkt);
        break;
        
        case ENODATA:
        packet_rollback(pkt);
        break;

        case EPLUGUP:
        packet_rollback(pkt);
        runnable_change_task(&p->r, plugin_alert_up);
        break;

        case EPLUGDOWN:
        packet_rollback(pkt);
        runnable_change_task((runnable_t *)p, plugin_alert_down);
        break;
    }

    if(packet_expire(pkt)) {
        if(pkt->state == WROTE) {
            packet_append(pkt, "]}");

            packet_t *new = packet_alloc(METRIC);
            packet_append(new, "{\"license\":\"%s\",\"tid\":%llu,\"metrics\":[", license, p->tid);
            new->state = BEGIN;
            pkt->next  = new;
            p->working = new;

            pkt->state = READY;
        } else {
            pkt->state = DONE;
        }
    }

    DEBUG(zlog_debug(p->tag, ".. %d bytes, in %llums", pkt->size, epoch_time()-begin));

    return 0;
}

int plugin_gather_phase(plugin_t *p) {
    return (void *)p->task == (void *)plugin_gather;
}
