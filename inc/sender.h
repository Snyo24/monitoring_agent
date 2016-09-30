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

#define BACKOFF_LIMIT 6

typedef runnable_t sender_t;
typedef struct _sender_spec {
	CURL *curl;
	struct curl_slist *headers;
	char response[1000];

	timestamp base_period;
	FILE *unsent_sending_fp;
	unsigned backoff : BACKOFF_LIMIT;
	char unsent_json[8192];
	unsigned unsent_json_loaded : 1;
} sender_spec_t;

extern sender_t sender;

int  sender_init(sender_t *sender);
void sender_fini(sender_t *sender);

void sender_main(void *_sender);

void sender_set_reg_uri(sender_t *sender);
void sender_set_met_uri(sender_t *sender);

int sender_post(sender_t *sender, char *payload);

#endif