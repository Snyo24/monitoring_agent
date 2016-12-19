/**
 * @file sender.h
 * @author Snyo 
 * @brief Send packets
 */

#ifndef _SENDER_H_
#define _SENDER_H_

#include <curl/curl.h>

#include "runnable.h"
#include "plugin.h"

#define REGISTER 0
#define METRIC   1
#define ALERT    2

typedef struct sender_t {
	runnable_t;

    int spin[3];
    CURL *curl[3];

	FILE *unsent_sending_fp;
	char unsent_json[41960];
	unsigned unsent_json_loaded : 1;

	unsigned backoff : 5;

    int  pluginc;
    void **plugins;
} sender_t;

int sender_init(sender_t *sndr, int pluginc, void **plugins);
int sender_fini(sender_t *sndr);

int sender_post(sender_t *sndr, const char *payload, int post_type);

int sender_main(void *_sender);

#endif
