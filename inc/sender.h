/**
 * @file sender.h
 * @author Snyo 
 * @brief Send packets
 */

#ifndef _SENDER_H_
#define _SENDER_H_

#include <curl/curl.h>

#include "runnable.h"

typedef struct sender_t {
	runnable_t;

	CURL *curl;
	struct curl_slist *header;

	FILE *unsent_sending_fp;
	char unsent_json[41960];
	unsigned unsent_json_loaded : 1;

	unsigned backoff : 5;
} sender_t;

int  sender_init(sender_t *sender);
void sender_fini(sender_t *sender);

void sender_main(void *_sender);

void sender_set_reg_uri(sender_t *sender);
void sender_set_met_uri(sender_t *sender);

int sender_post(sender_t *sender, const char *payload);
int alert_post(char *payload);

#endif
