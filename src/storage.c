/**
 * @file storage.c
 * @author Snyo
 */
#include "storage.h"

#include <stdio.h>
#include <string.h>

#include <zlog.h>

#include "routine.h"
#include "plugin.h"
#include "packet.h"
#include "sender.h"
#include "unsent.h"

#define STORAGE_TICK 21

int storage_main(void *_st);

static packet_t *packets;

int storage_init(routine_t *st) {
	if(routine_init(st) < 0) return -1;
	if(!(st->tag = zlog_get_category("storage")));

    if(sender_init() < 0)
        zlog_error(st->tag, "Fail to init sender");
    unsent_init();

    packets = NULL;
    
	st->delay = 1;

	st->tick = STORAGE_TICK;
	st->task = storage_main;

	return 0;
}

int storage_fini(routine_t *st) {
    while(packets) {
        packet_t *pkt = packets;
        packets = packets->next;
        packet_free(pkt);
    }
    sender_fini();
	routine_fini(st);
    return 0;
}

packet_t *get_packet(enum packet_type type) {
    for(packet_t *pkt=packets; pkt; pkt=pkt->next) {
        if(packet_change_state(pkt, DONE, EMPTY) == 0) {
            pkt->type = type;
            pkt->response = 0;
            pkt->size = 0;
            pkt->attempt = 0;
            pkt->spin = 0;
            return pkt;
        }
    }

    packet_t *pkt = packet_alloc(type);
    
    while(!packets) {
        if(__sync_bool_compare_and_swap(&packets, 0, pkt))
            return pkt;
    }

    int *head_lock = &packets->spin;
    while(!__sync_bool_compare_and_swap(head_lock, 0, 1));
    pkt->next = packets;
    packets = pkt;
    *head_lock = 0;

    return pkt;
}

int storage_main(void *_st) {
	routine_t *st = _st;

    for(packet_t *pkt=packets; pkt; pkt=pkt->next) {
        DEBUG(zlog_debug(st->tag, "-> %llx(%d)", (unsigned long long)pkt%0x10000, pkt->state));
        if(pkt->state == READY) {
            DEBUG(zlog_debug(st->tag, "%04llx: POST %.1fkB", (unsigned long long)pkt%0x10000, (float)pkt->size/BPKB));
            if(post(pkt) < 0) {
                DEBUG(zlog_debug(st->tag, "Fails (%d)", pkt->response));
                st->delay = (st->delay << 1) | !(st->delay << 1);
                if(pkt->type == METRIC && pkt->attempt >= 3) {
                    // TODO unsent_store(pkt);
                }
                return -1;
            } else {
                DEBUG(zlog_debug(st->tag, "Success"));
                st->delay = 1;
                // TODO unsent_send();
            }
            pkt->state = DONE;
            st->tick = STORAGE_TICK * st->delay;
        }

        //packet_change_state(pkt, DONE, FREE);
    }

    /*for(packet_t *prev=0, *pkt=packets; pkt; ) {
        if(pkt->state == FREE) {
        } else {
        }
    }*/

    return 0;
}
