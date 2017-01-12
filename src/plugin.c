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
#include "storage.h"
#include "util.h"

int plugin_fini(plugin_t *p);
int plugin_prep(plugin_t *p);
int plugin_regr(plugin_t *p);
int plugin_gather(plugin_t *p);

int plugin_init(plugin_t *p) {
    if(!p) return -1;

	if(routine_init(&p->r) < 0)
		return -1;

    p->tid = 0;
    p->working = 0;
    p->oob = 0;

    routine_change_task(&p->r, plugin_prep);

    return 0;
}

int plugin_fini(plugin_t *p) {
    if(!p) return -1;

    DEBUG(zlog_debug(p->tag, "Finialize"));

    if(p->fini) p->fini(p);
    packet_free(p->working);

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

    routine_change_task(&p->r, plugin_regr);

    return plugin_regr(p);
}

int plugin_regr(plugin_t *p) {
    if(!p) return -1;

    DEBUG(zlog_debug(p->tag, ".. Register"));

    if(!p->oob) {
        if(!p->tid)
            p->tid = epoch_time()*epoch_time()*epoch_time();

        p->oob = packet_alloc(REGISTER);
        p->oob->state = READY;
        packet_append(p->oob, "{\"license\":\"%s\",\"aid\":%llu,\"tid\":", license, aid);
        packet_transaction(p->oob);
        packet_append(p->oob, "%20llu,\"agent_type\":\"%s\",\"target_type\":\"%s\",\"os\":\"%s\",\"hostname\":\"%s\",\"ip\":\"%s\"%s%s%s}", p->tid, type, p->type, os, host, aip, p->tip?",\"target_ip\":\"":"", p->tip?p->tip:"", p->tip?"\"":"");
    } else {
        sprintf(p->oob->payload+p->oob->rollback_point, "%20llu", p->tid);
        p->oob->payload[p->oob->rollback_point+20] = ',';
    }

    if(post(p->oob) < 0) {
        switch(p->oob->response) {
            case EINVALLICENSE:
            break;

            case ETARGETAUTHFAIL:
            break;

            case EAGENTAUTHFAIL:
            break;

            case ETARGETREG:
            p->tid = epoch_time()*epoch_time()*epoch_time();
            break;

            case ETARGETPCH:
            break;

            case EAGENTREG:
            break;

            case EAGENTPCH:
            break;

            default:
            // unexpected response
            break;
        }
        return -1;
    }

    packet_free(p->oob);
    p->oob = 0;

    char m_name[BFSZ];
    snprintf(m_name, BFSZ, "res/%llu", p->tid);

    FILE *fp = fopen(m_name, "w+");
    int type_size = strlen(p->type);
    fwrite(&type_size, 1, sizeof(int), fp);
    fwrite(p->type, 1, type_size, fp);
    fwrite(&p->module_size, 1, sizeof(int), fp);
    fwrite(p->module, 1, p->module_size, fp);
    fclose(fp);

    routine_change_task(&p->r, plugin_gather);

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
    routine_change_task(&p->r, plugin_gather);

    return 0;
}

int plugin_alert_up(plugin_t *p) {
    return plugin_alert(p, "up");
}

int plugin_alert_down(plugin_t *p) {
    return plugin_alert(p, "down");
}

int plugin_gather(plugin_t *p) {
    DEBUG(zlog_debug(p->tag, ".. Gather"));

    if(!p->working)
        p->working = get_packet(METRIC);

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
        routine_change_task(&p->r, plugin_alert_up);
        break;

        case EPLUGDOWN:
        packet_rollback(pkt);
        routine_change_task(&p->r, plugin_alert_down);
        break;
    }

    if(packet_expired(pkt)) {
        if(pkt->state == WROTE) {
            packet_append(pkt, "]}");
            pkt->state = READY;
            p->working = 0;
        } else {
            pkt->state = DONE;
            p->working = 0;
        }
    }

    DEBUG(zlog_debug(p->tag, ".. %d bytes, in %llums", pkt->size, epoch_time()-begin));

    return 0;
}

int plugin_gather_phase(plugin_t *p) {
    return (unsigned long int *)p->task == (unsigned long int *)plugin_gather;
}
