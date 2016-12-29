/**
 * @file storage.c
 * @author Snyo
 */

#include "storage.h"

#include <stdio.h>
#include <string.h>

#include <zlog.h>

#include "runnable.h"
#include "plugin.h"
#include "packet.h"
#include "sender.h"
#include "unsent.h"
#include "util.h"

#define STORAGE_TICK 19

int storage_main(void *_st);

int storage_init(runnable_t *st) {
	if(runnable_init(st) < 0) return -1;
	if(!(st->tag = zlog_get_category("storage")));

    if(sender_init() < 0)
        zlog_error(st->tag, "Fail to init sender");
    unsent_init();
    
	st->delay   = 1;

	st->tick = STORAGE_TICK;
	st->task = storage_main;

	return 0;
}

int storage_fini(runnable_t *st) {
    sender_fini();
	runnable_fini(st);
    return 0;
}

int storage_main(void *_st) {
	runnable_t *st = (runnable_t *)_st;

    extern int pluginc;
    extern plugin_t *plugins[];

    for(int i=0; i<pluginc; i++) {
        plugin_t *p = plugins[i];
        if(!p) continue;

        packet_t *pkt = p->packets;
        while(pkt) {
            printf("-> %04lld(%d) ", pkt->started%10000, pkt->state);
            if(pkt->state == READY) {
                if(post(pkt) < 0) {
                    st->delay = (st->delay << 1) | !(st->delay << 1);
                    st->tick = STORAGE_TICK * st->delay;

                    if(pkt->type == METRIC && pkt->attempt >= 3) {
                        // TODO zlog_unsent(st->tag, "%s", pkt->payload);
                        pkt->state = DONE;
                    }
                    return -1;
                } else {
                    st->delay = 1;
                    st->tick = STORAGE_TICK;

                    pkt->state = DONE;
                    // TODO unsent_send();
                }
                DEBUG(zlog_debug(st->tag, "POST %.1fkB, %lld", (float)pkt->size/BPKB, pkt->started%10000));
            }
            pkt = pkt->next;
        }
        printf("\n");
	}

    return 0;
}
