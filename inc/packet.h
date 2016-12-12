#ifndef _PACKET_H_
#define _PACKET_H_

#include "util.h"

#define PCKTSZ 32768

typedef struct packet_t {
    unsigned ready : 1;
    unsigned attempt : 2;
    epoch_t created;

    int ofs;
    char payload[PCKTSZ];
    
    struct packet_t *next;
    struct packet_t *prev;
} packet_t;

packet_t *alloc_packet();
void free_packet(packet_t *p);

int packet_new(packet_t *p);
int packet_expire(packet_t *p);

#endif
