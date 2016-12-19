#include "packet.h"

#include <string.h>
#include <stdlib.h>

#include "util.h"

packet_t *alloc_packet() {
    packet_t *p = malloc(sizeof(packet_t));
    if(!p) return NULL;
    memset(p, 0, sizeof(packet_t));
    p->created = epoch_time();

    return p;
}

void free_packet(packet_t *p) {
    if(p) free(p);
}

char *packet_fetch(packet_t *p) {
    if(!p || !p->ready)
        return NULL;
    return p->payload;
}

unsigned packet_new(packet_t *p) {
    return p && (epoch_time()-p->created) <= MSPS;
}

unsigned packet_expire(packet_t *p) {
    return p && (epoch_time()-p->created) >= 6*MSPS;
}
