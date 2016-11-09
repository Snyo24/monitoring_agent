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

#define REG_URI    "http://gate.maxgauge.com/v1/agents"
#define METRIC_URI "http://gate.maxgauge.com/v1/metrics"
#define ALERT_URI  "http://gate.maxgauge.com/v1/alert"

#define CONTENT_TYPE "Content-Type: application/vnd.exem.v1+json"

#define UNSENT_SENDING "log/unsent_sending"
#define UNSENT_BEGIN -1
#define UNSENT_END   0 // TODO must control with configuration file

char unsent_path[34] = "log";
char unsent_name[50];
char unsent_end[50];
char unsent_sending[50];

CURL *curl_alert;
struct curl_slist *header;

static int clear_unsent();
static int load_unsent(sender_t *sender);
static void drop_unsent_sending(sender_t *sender);
static char *unsent_file(int i);
static size_t post_callback(char *ptr, size_t size, size_t nmemb, void *tag);

int sender_init(sender_t *sender) {
	if(runnable_init((runnable_t *)sender) < 0) return -1;
	if(!(sender->tag = zlog_get_category("sender")));

	zlog_debug(sender->tag, "Clear old data");
	if(clear_unsent() < 0)
		zlog_warn(sender->tag, "Fail to clear old data");

	snprintf(unsent_end, 50, "%s/unsent.%d", unsent_path, UNSENT_END);
	snprintf(unsent_sending, 50, "%s/unsent_sending", unsent_path);

	zlog_debug(sender->tag, "Initialize cURL");
	sender->header = 0;
	if(!(sender->curl = curl_easy_init())
		|| !(sender->header = curl_slist_append(sender->header, CONTENT_TYPE))
		|| curl_easy_setopt(sender->curl, CURLOPT_TIMEOUT,       30)            != CURLE_OK
		|| curl_easy_setopt(sender->curl, CURLOPT_NOSIGNAL,      1)             != CURLE_OK
		|| curl_easy_setopt(sender->curl, CURLOPT_WRITEDATA,     sender->tag)   != CURLE_OK
		|| curl_easy_setopt(sender->curl, CURLOPT_HTTPHEADER,    sender->header)!= CURLE_OK
		|| curl_easy_setopt(sender->curl, CURLOPT_WRITEFUNCTION, post_callback) != CURLE_OK) {
		zlog_error(sender->tag, "Fail to setup cURL");
		sender_fini(sender);
		return -1;
	}

	header = 0;
	if(!(curl_alert = curl_easy_init())
		|| !(header = curl_slist_append(header, CONTENT_TYPE))
		|| curl_easy_setopt(curl_alert, CURLOPT_TIMEOUT,       30)            != CURLE_OK
		|| curl_easy_setopt(curl_alert, CURLOPT_NOSIGNAL,      1)             != CURLE_OK
		|| curl_easy_setopt(curl_alert, CURLOPT_WRITEDATA,     sender->tag)   != CURLE_OK
		|| curl_easy_setopt(curl_alert, CURLOPT_URL, ALERT_URI) != CURLE_OK
		|| curl_easy_setopt(curl_alert, CURLOPT_HTTPHEADER,    header)        != CURLE_OK
		|| curl_easy_setopt(curl_alert, CURLOPT_WRITEFUNCTION, post_callback) != CURLE_OK) {
		zlog_error(sender->tag, "Fail to setup cURL");
		sender_fini(sender);
		return -1;
	}

	sender->period = SENDER_TICK;
	sender->backoff = 1;
	sender->unsent_sending_fp = NULL;
	sender->unsent_json_loaded = 0;

	sender->job = sender_main;

	return 0;
}

void sender_fini(sender_t *sender) {
	runnable_fini((runnable_t *)sender);
	curl_slist_free_all(sender->header);
	curl_slist_free_all(header);
	curl_easy_cleanup(sender->curl);
}

void sender_main(void *_sender) {
	sender_t *sender = (sender_t *)_sender;

	while(!storage_empty(&storage)) {
		char *payload = storage_fetch(&storage);
		zlog_debug(sender->tag, "Try POST for 30sec");
		if(sender_post(sender, payload) < 0) {
			zlog_debug(sender->tag, "POST fail");
			sender->backoff <<= 1;
			sender->backoff |= !sender->backoff;
			sender->period = SENDER_TICK * sender->backoff;
			break;
		} else {
			zlog_debug(sender->tag, "POST success");
			storage_drop(&storage);
			sender->period = SENDER_TICK;
			sender->backoff = 1;

			if(!sender->unsent_sending_fp) {
				if(load_unsent(sender) < 0) continue;
			} else if(file_exist(unsent_end)) {
				drop_unsent_sending(sender);
				if(load_unsent(sender) < 0) continue;
			}

			zlog_debug(sender->tag, "POST unsent JSON");
			while(sender->unsent_json_loaded 
			   || fgets(sender->unsent_json, 41960, sender->unsent_sending_fp)) {
				sender->unsent_json_loaded = 1;
				if(sender_post(sender, sender->unsent_json) < 0) {
					zlog_debug(sender->tag, "POST unsent fail");
					return;
				}
				sender->unsent_json_loaded = 0;
			}
			zlog_debug(sender->tag, "Fail to get unsent JSON");
			drop_unsent_sending(sender);
		}
	}
}

void sender_set_reg_uri(sender_t *sender) {
	curl_easy_setopt(sender->curl, CURLOPT_URL, REG_URI);
}

void sender_set_met_uri(sender_t *sender) {
	curl_easy_setopt(sender->curl, CURLOPT_URL, METRIC_URI);
}

int alert_post(char *payload) {
	curl_easy_setopt(curl_alert, CURLOPT_POSTFIELDS, payload);
	CURLcode curl_code = curl_easy_perform(curl_alert);
	long status_code;
	curl_easy_getinfo(curl_alert, CURLINFO_RESPONSE_CODE, &status_code);
	if(status_code == 403) {
		exit(1);
	}
	return (curl_code == CURLE_OK && status_code == 202) - 1;
}

int sender_post(sender_t *sender, char *payload) {
	zlog_debug(sender->tag, "%s (%zu)", payload, strlen(payload));
	curl_easy_setopt(sender->curl, CURLOPT_POSTFIELDS, payload);
	CURLcode curl_code = curl_easy_perform(sender->curl);
	long status_code;
	curl_easy_getinfo(sender->curl, CURLINFO_RESPONSE_CODE, &status_code);
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
	if(!file_exist(unsent_file(UNSENT_BEGIN)) && !file_exist(unsent_file(0)))
		return -1;
	for(int i=UNSENT_END; i>=UNSENT_BEGIN; --i) {
		char *unsent = unsent_file(i);
		if(file_exist(unsent)) {
			if(rename(unsent, unsent_sending))
				return -1;
			sender->unsent_sending_fp = fopen(unsent_sending, "r");
			return (sender->unsent_sending_fp != NULL)-1;
		}
	}
	return -1;
}

void drop_unsent_sending(sender_t *sender) {
	zlog_debug(sender->tag, "Close and remove unsent_sending");
	fclose(sender->unsent_sending_fp);
	sender->unsent_sending_fp = NULL;
	sender->unsent_json_loaded = 0;
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
