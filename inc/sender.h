/**
 * @file sender.h
 * @author Snyo 
 * @brief Send packets
 */

#ifndef _SENDER_H_
#define _SENDER_H_

#include <curl/curl.h>

#include "runnable.h"
#include "util.h"

typedef runnable_t sender_t;

int  sender_init(sender_t *sender);
void sender_fini(sender_t *sender);

void sender_main(void *_sender);

void sender_set_reg_uri(sender_t *sender);
void sender_set_met_uri(sender_t *sender);

int sender_post(sender_t *sender, char *payload);

#endif
