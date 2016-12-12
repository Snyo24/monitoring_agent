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
    free(p);
}

int packet_new(packet_t *p) {
    return p && (epoch_time()-p->created) <= MSPS;
}

int packet_expire(packet_t *p) {
    return p && (epoch_time()-p->created) >= 6*MSPS;
}
