#include "packet.h"

#include <string.h>
#include <stdlib.h>

#include "util.h"

#define PACKET_EXP 8.033F

packet_t *packet_alloc(int type) {
    if(type < 0 || type >=3)
        return NULL;

    packet_t *pkt = malloc(sizeof(packet_t));
    if(!pkt) return NULL;
    memset(pkt, 0, sizeof(packet_t));

    pkt->type  = type;
    pkt->state = EMPTY;

    return pkt;
}

void packet_free(packet_t *pkt) {
    if(pkt) free(pkt);
}

char *packet_fetch(packet_t *pkt) {
    char *payload = NULL;

    while(!__sync_bool_compare_and_swap(&pkt->spin, 0, 1));
    if(pkt && pkt->state==READY)
        payload = pkt->payload;
    pkt->spin = 0;

    return payload;
}

int packet_expired(packet_t *pkt) {
    return pkt && (epoch_time()-pkt->started)/MSPS >= PACKET_EXP;
}
