/**
 * @file sender.h
 * @author Snyo 
 * @brief Send packets
 */

#ifndef _SENDER_H_
#define _SENDER_H_

#include <curl/curl.h>

#include "packet.h"

typedef struct sender_t {
    CURL *curl;
    int spin;
} sender_t;

int sender_init();
int sender_fini();

int post(packet_t *pkt);

#endif
