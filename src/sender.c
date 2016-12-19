/**
 * @file sndr.c
 * @author Snyo
 */

#include "sender.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <zlog.h>
#include <curl/curl.h>

#include "packet.h"
#include "util.h"

#define SENDER_TICK 1

#define REGISTER_URL "http://52.79.75.180:8080/v1/agents"
#define METRIC_URL   "http://52.79.75.180:8080/v1/metrics"
#define ALERT_URL    "http://52.79.75.180:8080/v1/alert"

#define CONTENT_TYPE "Content-Type: application/vnd.exem.v1+json"

#define UNSENT_SENDING "log/unsent_sending"
#define UNSENT_BEGIN -1
#define UNSENT_END   0 // TODO must control with configuration file

char unsent_path[34] = "log";
char unsent_name[50];
char unsent_end[50];
char unsent_sending[50];

struct curl_slist *header;

static size_t callback(char *ptr, size_t size, size_t nmemb, void *tag);
static int clear_unsent();
static int load_unsent(sender_t *sndr);
static void drop_unsent_sending(sender_t *sndr);
static char *unsent_file(int i);
static int curl_init(CURL **curl, const char *url);

int sender_init(sender_t *sndr, int pluginc, void **plugins) {
	if(runnable_init((runnable_t *)sndr) < 0) return -1;
	if(!(sndr->tag = zlog_get_category("sender")));

	DEBUG(zlog_debug(sndr->tag, "Clear old data"));
	if(clear_unsent() < 0)
		zlog_warn(sndr->tag, "Fail to clear old data");

	snprintf(unsent_end, 50, "%s/unsent.%d", unsent_path, UNSENT_END);
	snprintf(unsent_sending, 50, "%s/unsent_sending", unsent_path);

    curl_global_init(CURL_GLOBAL_DEFAULT);

	DEBUG(zlog_debug(sndr->tag, "Initialize cURL"));
	header = 0;
    header = curl_slist_append(header, CONTENT_TYPE);
    if(!header) {
        zlog_error(sndr->tag, "Fail to initialize header");
        return -1;
    }

    if(0x00 || curl_init(sndr->curl+REGISTER, REGISTER_URL) < 0
            || curl_init(sndr->curl+METRIC,   METRIC_URL)   < 0
            || curl_init(sndr->curl+ALERT,    ALERT_URL)    < 0) {
        curl_easy_cleanup(sndr->curl[REGISTER]);
        curl_easy_cleanup(sndr->curl[METRIC]);
        curl_easy_cleanup(sndr->curl[ALERT]);
        zlog_error(sndr->tag, "Fail to setup cURL");
        return -1;
    }

	sndr->tick = SENDER_TICK;
	sndr->backoff = 1;
	sndr->unsent_sending_fp = NULL;
	sndr->unsent_json_loaded = 0;
    sndr->pluginc = pluginc;
    sndr->plugins = plugins;

	sndr->routine = sender_main;

	return 0;
}

int curl_init(CURL **curl, const char *url) {
    return ((*curl = curl_easy_init())
            && curl_easy_setopt(*curl, CURLOPT_URL, url) == CURLE_OK
            && curl_easy_setopt(*curl, CURLOPT_TIMEOUT, 30) == CURLE_OK
            && curl_easy_setopt(*curl, CURLOPT_NOSIGNAL, 1) == CURLE_OK
            && curl_easy_setopt(*curl, CURLOPT_HTTPHEADER, header) == CURLE_OK
            && curl_easy_setopt(*curl, CURLOPT_WRITEFUNCTION, callback) == CURLE_OK) - 1;
}

int sender_fini(sender_t *sndr) {
	runnable_fini((runnable_t *)sndr);
	curl_slist_free_all(header);
	curl_easy_cleanup(sndr->curl);
    return 0;
}

int sender_post(sender_t *sndr, const char *payload, int post_type) {
	DEBUG(zlog_debug(sndr->tag, "%s", payload));
    while(__sync_bool_compare_and_swap(&sndr->spin[post_type], 0, 1));
	curl_easy_setopt(sndr->curl[post_type], CURLOPT_POSTFIELDS, payload);
	CURLcode curl_code = curl_easy_perform(sndr->curl);
    long status_code;
	curl_easy_getinfo(sndr->curl, CURLINFO_RESPONSE_CODE, &status_code);
    sndr->spin[post_type] = 0;
	DEBUG(zlog_debug(sndr->tag, "POST %.1fkB, curl(%d), http(%ld)", (float)strlen(payload)/1024, curl_code, status_code));
	if(status_code == 403) {
		zlog_error(sndr->tag, "Cannot verify your license");
        return status_code;
	}
	return (curl_code==CURLE_OK && status_code==202) - 1;
}

size_t callback(char *ptr, size_t size, size_t nmemb, void *tag) {
	printf("%.*s\n", (int)size*(int)nmemb, ptr);
	return nmemb;
}

int clear_unsent() {
	int success = 0;
	for(int i=UNSENT_BEGIN; i<=UNSENT_END; ++i) {
		char *unsent = unsent_file(i);
		if(file_exist(unsent))
			success |= remove(unsent);
	}
	if(file_exist(UNSENT_SENDING))
		success |= remove(UNSENT_SENDING);

	return !success - 1;
}

int load_unsent(sender_t *sndr) {
	if(!file_exist(unsent_file(UNSENT_BEGIN)) && !file_exist(unsent_file(0)))
		return -1;
	for(int i=UNSENT_END; i>=UNSENT_BEGIN; --i) {
		char *unsent = unsent_file(i);
		if(file_exist(unsent)) {
			if(rename(unsent, unsent_sending))
				return -1;
			sndr->unsent_sending_fp = fopen(unsent_sending, "r");
			return (sndr->unsent_sending_fp != NULL)-1;
		}
	}
	return -1;
}

void drop_unsent_sending(sender_t *sndr) {
	DEBUG(zlog_debug(sndr->tag, "Close and remove unsent_sending"));
	fclose(sndr->unsent_sending_fp);
	sndr->unsent_sending_fp = NULL;
	sndr->unsent_json_loaded = 0;
	remove(unsent_sending);
}

char *unsent_file(int i) {
	if(i >= 0) {
		snprintf(unsent_name, 50, "%s/unsent.%d", unsent_path, i);
	} else {
		snprintf(unsent_name, 50, "%s/unsent", unsent_path);
	}
	return unsent_name;
}

int sender_main(void *_sender) {
	sender_t *sndr = (sender_t *)_sender;

    for(int i=0; i<sndr->pluginc; i++) {
        plugin_t *plugin = sndr->plugins[i];
        if(!plugin) continue;
        packet_t *packet = plugin->packets;
        char *payload = packet_fetch(plugin->packets);
        if(!payload) continue;
		DEBUG(zlog_debug(sndr->tag, "Try POST for 30sec"));
		if(sender_post(sndr, payload, METRIC) < 0) {
			zlog_error(sndr->tag, "POST fail");
            packet->attempt++;
            if(packet->attempt >= 3) {
                plugin->packets = packet->next;
                free_packet(packet);
            }
			sndr->backoff <<= 1;
			sndr->backoff |= !sndr->backoff;
			sndr->tick = SENDER_TICK * sndr->backoff;
			break;
		} else {
			DEBUG(zlog_debug(sndr->tag, "POST success"));
            plugin->packets = packet->next;
            free_packet(packet);
			sndr->tick = SENDER_TICK;
			sndr->backoff = 1;

			/*if(!sndr->unsent_sending_fp) {
				if(load_unsent(sndr) < 0) continue;
			} else if(file_exist(unsent_end)) {
				drop_unsent_sending(sndr);
				if(load_unsent(sndr) < 0) continue;
			}

			DEBUG(zlog_debug(sndr->tag, "POST unsent JSON"));
			while(sndr->unsent_json_loaded 
					|| fgets(sndr->unsent_json, 41960, sndr->unsent_sending_fp)) {
				sndr->unsent_json_loaded = 1;
				if(sender_post(sndr, sndr->unsent_json, METRIC) < 0) {
					zlog_error(sndr->tag, "POST unsent fail");
					return;
				}
				sndr->unsent_json_loaded = 0;
			}
			zlog_error(sndr->tag, "Fail to get unsent JSON");
			drop_unsent_sending(sndr);*/
		}
	}
    return 0;
}
