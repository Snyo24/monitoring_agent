#include "sender.h"
#include "util.h"

#include <stdlib.h>
#include <stdbool.h>

#include <pthread.h>
#include <zlog.h>
#include <curl/curl.h>

#define SENDER_TICK NANO/5
#define KAFKA_SERVER "http://10.10.202.115:8082/v1/agents"
#define UNSENT_NUMBER 2 // TODO must control with configuration file
#define zlog_unsent(cat, format, ...) \
                   (zlog(cat,__FILE__,sizeof(__FILE__)-1,__func__,sizeof(__func__)-1,__LINE__, \
                    255,format,##__VA_ARGS__))

char *deq_payload();
bool sender_empty();
bool sender_full();
bool load_unsent();
void drop_unsent();
void double_backoff();

void sender_init() {
	g_sender = NULL;
	zlog_category_t *_tag = zlog_get_category("Sender");
    if (!_tag) return;
	g_sender = (sender_t *)malloc(sizeof(sender_t));
	if(!g_sender) return;

	g_sender->alive = 1;

	zlog_debug(_tag, "Setup cURL");
	g_sender->curl = curl_easy_init();
	if(!g_sender->curl) {
		zlog_error(_tag, "Fail to setup cURL");
		free(g_sender);
		return;
	} else {
		zlog_debug(_tag, "Setup cURL header");
		g_sender->headers = NULL;
		g_sender->headers = curl_slist_append(g_sender->headers, "Accept: application/json");
		g_sender->headers = curl_slist_append(g_sender->headers, "Content-Type: application/vnd.exem.v1+json");

		zlog_debug(_tag, "Setup cURL option");
		curl_easy_setopt(g_sender->curl, CURLOPT_URL, KAFKA_SERVER);
		curl_easy_setopt(g_sender->curl, CURLOPT_HTTPHEADER, g_sender->headers);
		curl_easy_setopt(g_sender->curl, CURLOPT_TIMEOUT, 1);
	}

	g_sender->head = 0;
	g_sender->tail = 0;
	g_sender->holding = 0;
	
	g_sender->tag = _tag;

	g_sender->unsent_fp = NULL;
	g_sender->backoff = 1;

	pthread_mutex_init(&g_sender->enqmtx, NULL);
	pthread_create(&g_sender->running_thread, NULL, sender_run, NULL);
}

void sender_fini() {
	zlog_debug(g_sender->tag, "Cleaning up");
	while(!sender_empty())
		free(deq_payload());
	curl_slist_free_all(g_sender->headers);
	curl_easy_cleanup(g_sender->curl);
}

void *sender_run(void *__unused) {
	char unsent_MAX[20];
	snprintf(unsent_MAX, 20, "log/unsent.%d", UNSENT_NUMBER-1);

	while(g_sender->alive) {
		zlog_debug(g_sender->tag, "Sender has %d/%d JSON", g_sender->holding, MAX_HOLDING+EXTRA_HOLDING);
		if(sender_full()) {
			zlog_debug(g_sender->tag, "Store unsent JSON");
			pthread_mutex_lock(&g_sender->enqmtx);
			while(!sender_empty()) {
				char *payload = deq_payload();
				zlog_unsent(g_sender->tag, "%s", payload);
				free(payload);
			}
			pthread_mutex_unlock(&g_sender->enqmtx);
		} else if(!sender_empty()) {
			zlog_debug(g_sender->tag, "Try POST (timeout: 1s)");
			if(sender_post(g_sender->queue[g_sender->head])) {
				zlog_debug(g_sender->tag, "POST success");
				free(deq_payload());
				g_sender->backoff = 1;

				if(!g_sender->unsent_fp) {
					if(!load_unsent()) continue;
				} else if(file_exist(unsent_MAX)) {
					drop_unsent();
					if(!load_unsent()) continue;
				}

				zlog_debug(g_sender->tag, "POST unsent JSON");
				for(int i=0; i<MAX_HOLDING; ++i) {
					char buf[4096];
					if(!fgets(buf, 4096, g_sender->unsent_fp)) {
						zlog_debug(g_sender->tag, "fgets() fail");
						drop_unsent();
						break;
					} else if(!sender_post(buf)) {
						zlog_debug(g_sender->tag, "POST fail");
						break;
					}
				}
			} else {
				zlog_debug(g_sender->tag, "POST fail");
				zlog_debug(g_sender->tag, "Sleep for %lums", g_sender->backoff*SENDER_TICK/1000000);
				snyo_sleep(g_sender->backoff*SENDER_TICK);
				double_backoff();
			}
		} else {
			zlog_debug(g_sender->tag, "Sleep for %lums", SENDER_TICK/1000000);
			snyo_sleep(SENDER_TICK);
		}
	}
	return NULL;
}

bool sender_post(char *payload) {
	curl_easy_setopt(g_sender->curl, CURLOPT_POSTFIELDS, payload);
	long status_code;
	CURLcode curl_code = curl_easy_perform(g_sender->curl);
	curl_easy_getinfo(g_sender->curl, CURLINFO_RESPONSE_CODE, &status_code);
	return curl_code == CURLE_OK && status_code == 202;
}

void enq_payload(char *payload) {
	pthread_mutex_lock(&g_sender->enqmtx);
	zlog_error(g_sender->tag, "Queuing Idx: %d", g_sender->tail);
	g_sender->queue[g_sender->tail++] = payload;
	g_sender->tail %= (MAX_HOLDING+EXTRA_HOLDING);
	g_sender->holding++;
	pthread_mutex_unlock(&g_sender->enqmtx);
}

bool load_unsent() {
	for(int i=UNSENT_NUMBER-1; i>=0; --i) {
		char unsent_log[20];
		snprintf(unsent_log, 20, "log/unsent.%d", i);
		if(file_exist(unsent_log)) {
			if(rename(unsent_log, "log/unsent_sending"))
				return false;
			g_sender->unsent_fp = fopen("log/unsent_sending", "r");
			return g_sender->unsent_fp != NULL;
		}
	}
	if(!g_sender->unsent_fp) {
		if(file_exist("log/unsent")) {
			if(rename("log/unsent", "log/unsent_sending"))
				return false;
			g_sender->unsent_fp = fopen("log/unsent_sending", "r");
			return g_sender->unsent_fp != NULL;
		}
	}
	return false;
}

void drop_unsent() {
	zlog_debug(g_sender->tag, "Close and remove unsent_sending");
	fclose(g_sender->unsent_fp);
	remove("log/unsent.sending");
	g_sender->unsent_fp = NULL;
}

char *deq_payload() {
	char *ret = g_sender->queue[g_sender->head++];
	g_sender->head %= (MAX_HOLDING+EXTRA_HOLDING);
	g_sender->holding--;
	return ret;
}

bool sender_empty() {
	return g_sender->holding == 0;
}

bool sender_full() {
	return g_sender->holding >= MAX_HOLDING;
}

void double_backoff() {
	g_sender->backoff <<= 1;
	g_sender->backoff |= !g_sender->backoff;
}