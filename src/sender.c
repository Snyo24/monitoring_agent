/**
 * @file sender.c
 * @author Snyo
 */

#include "sender.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <zlog.h>
#include <curl/curl.h>
 
#include "storage.h"
#include "util.h"

#define SENDER_TICK MS_PER_S*3

#define REG_URI "http://gate.maxgauge.com/v1/agents"
#define METRIC_URI "http://gate.maxgauge.com/v1/metrics"

#define UNSENT_SENDING "log/unsent_sending"
#define UNSENT_BEGIN -1
#define UNSENT_END 0 // TODO must control with configuration file

#define CONTENT_TYPE "Content-Type: application/vnd.exem.v1+json"

#define BACKOFF_LIMIT 6

typedef struct _sender_spec {
	timestamp base_period;

	CURL              *curl;
	struct curl_slist *headers;

	FILE *unsent_sending_fp;
	char unsent_json[81920];
	unsigned unsent_json_loaded : 1;

	unsigned backoff : BACKOFF_LIMIT;
} sender_spec_t;

static int clear_unsent();
static int load_unsent(sender_t *sender);
static void drop_unsent_sending(sender_t *sender);
static char *unsent_file(int i);
static size_t post_callback(char *ptr, size_t size, size_t nmemb, void *tag);

int sender_init(sender_t *sender) {
	if(runnable_init(sender, SENDER_TICK) < 0) return -1;
	if(!(sender->tag = zlog_get_category("sender")));

	zlog_debug(sender->tag, "Clear old data");
	if(clear_unsent() < 0)
		zlog_warn(sender->tag, "Fail to clear old data");

	sender_spec_t *spec = malloc(sizeof(sender_spec_t));
	if(!spec) return -1;

	zlog_debug(sender->tag, "Initialize cURL");
	spec->curl = curl_easy_init();
	spec->headers = 0;
	if(!(spec->curl = curl_easy_init())
		|| !(spec->headers = curl_slist_append(spec->headers, CONTENT_TYPE))
		|| curl_easy_setopt(spec->curl, CURLOPT_NOSIGNAL, 1) != CURLE_OK
		|| curl_easy_setopt(spec->curl, CURLOPT_HTTPHEADER, spec->headers) != CURLE_OK
		|| curl_easy_setopt(spec->curl, CURLOPT_WRITEFUNCTION, post_callback) != CURLE_OK
		|| curl_easy_setopt(spec->curl, CURLOPT_WRITEDATA, sender->tag) != CURLE_OK
		|| curl_easy_setopt(spec->curl, CURLOPT_TIMEOUT, 30) != CURLE_OK) {
		zlog_error(sender->tag, "Fail to setup cURL");
		sender_fini(sender);
		return -1;
	}
	spec->base_period = SENDER_TICK;
	spec->backoff = 1;
	spec->unsent_sending_fp = NULL;
	spec->unsent_json_loaded = 0;

	sender->spec = spec;
	sender->job = sender_main;

	return 0;
}

void sender_fini(sender_t *sender) {
	sender_spec_t *spec = (sender_spec_t *)sender->spec;
	curl_easy_cleanup(spec->curl);
	curl_slist_free_all(spec->headers);
	free(sender->spec);
}

void sender_main(void *_sender) {
	sender_t *sender = (sender_t *)_sender;
	sender_spec_t *spec = (sender_spec_t *)sender->spec;

	char unsent_end[20];
	snprintf(unsent_end, 20, "log/unsent.%d", UNSENT_END);

	while(!storage_empty(&storage)) {
		zlog_debug(sender->tag, "Try POST (timeout: 1s)");
		char *payload = storage_fetch(&storage);
		if(!sender_post(sender, payload)) {
			zlog_debug(sender->tag, "POST success");
			storage_drop(&storage);
			sender->period = spec->base_period;

			if(!spec->unsent_sending_fp) {
				if(load_unsent(sender) < 0) continue;
			} else if(file_exist(unsent_end)) {
				drop_unsent_sending(sender);
				if(load_unsent(sender) < 0) continue;
			}

			zlog_debug(sender->tag, "POST %d unsent JSON", 3);
			for(int i=0; i<3; ++i) {
				if(!spec->unsent_json_loaded) {
					if(!fgets(spec->unsent_json, 8192, spec->unsent_sending_fp)) {
						zlog_debug(sender->tag, "fgets() fail");
						drop_unsent_sending(sender);
						break;
					}
				}
				spec->unsent_json_loaded = 1;
				if(sender_post(sender, spec->unsent_json) < 0) {
					zlog_debug(sender->tag, "POST unsent fail");
					break;
				}
				spec->unsent_json_loaded = 0;
			}
		} else {
			zlog_debug(sender->tag, "POST fail");
			spec->backoff <<= 1;
			spec->backoff |= !spec->backoff;
			sender->period = spec->base_period * spec->backoff;
			break;
		}
	}
}

void sender_set_reg_uri(sender_t *sender) {
	sender_spec_t *spec = (sender_spec_t *)sender->spec;
	curl_easy_setopt(spec->curl, CURLOPT_URL, REG_URI);
}

void sender_set_met_uri(sender_t *sender) {
	sender_spec_t *spec = (sender_spec_t *)sender->spec;
	curl_easy_setopt(spec->curl, CURLOPT_URL, METRIC_URI);
}

int sender_post(sender_t *sender, char *payload) {
	zlog_debug(sender->tag, "%s (%zu)", payload, strlen(payload));
	CURL *curl = ((sender_spec_t *)sender->spec)->curl;
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
	CURLcode curl_code = curl_easy_perform(curl);
	long status_code;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
	zlog_debug(sender->tag, "POST returns curl code(%d) and http_status(%ld)", curl_code, status_code);
	if(status_code == 403) {
		zlog_error(sender->tag, "Check your license");
		exit(1);
	}
	return (curl_code == CURLE_OK && status_code == 202) - 1;
}

size_t post_callback(char *ptr, size_t size, size_t nmemb, void *tag) {
	zlog_debug(tag, "%.*s\n", (int)size*(int)nmemb, ptr);
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

int load_unsent(sender_t *sender) {
	sender_spec_t* spec = (sender_spec_t *)sender->spec;
	if(!file_exist(unsent_file(UNSENT_BEGIN)) && !file_exist(unsent_file(0)))
		return -1;
	for(int i=UNSENT_END; i>=UNSENT_BEGIN; --i) {
		char *unsent = unsent_file(i);
		if(file_exist(unsent)) {
			if(rename(unsent, UNSENT_SENDING))
				return -1;
			spec->unsent_sending_fp = fopen(UNSENT_SENDING, "r");
			return (spec->unsent_sending_fp != NULL)-1;
		}
	}
	return -1;
}

void drop_unsent_sending(sender_t *sender) {
	sender_spec_t* spec = (sender_spec_t *)sender->spec;
	zlog_debug(sender->tag, "Close and remove unsent_sending");
	fclose(spec->unsent_sending_fp);
	spec->unsent_sending_fp = NULL;
	spec->unsent_json_loaded = 0;
	remove(UNSENT_SENDING);
}

char unsent_name[16] = "log/unsent";
char *unsent_file(int i) {
	if(i >= 0) {
		snprintf(unsent_name+10, 5, ".%i", i);
	} else {
		unsent_name[10] = '\0';
	}
	return unsent_name;
}
