#include "sender.h"
#include "util.h"

#include <stdlib.h>
#include <stdbool.h>

#include <pthread.h>
#include <zlog.h>
#include <curl/curl.h>

bool sender_empty();
bool sender_full();

void sender_init(const char *server) {
	g_sender = NULL;
	zlog_category_t *_tag = zlog_get_category("Sender");
    if (!_tag) return;
	g_sender = (sender_t *)malloc(sizeof(sender_t));
	if(!g_sender) return;

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
		g_sender->headers = curl_slist_append(g_sender->headers, "Content-Type: application/json");

		zlog_debug(_tag, "Setup cURL option");
		curl_easy_setopt(g_sender->curl, CURLOPT_URL, server);
		curl_easy_setopt(g_sender->curl, CURLOPT_HTTPHEADER, g_sender->headers);
		curl_easy_setopt(g_sender->curl, CURLOPT_TIMEOUT_MS, 1000);
	}
	g_sender->tag = _tag;
	g_sender->head = 0;
	g_sender->tail = 0;
	g_sender->holding = 0;

	pthread_mutex_init(&g_sender->mutex, NULL);
	pthread_create(&g_sender->running_thread, NULL, sender_run, NULL);
}

void sender_fini() {
	zlog_debug(g_sender->tag, "Cleaning up");
	curl_slist_free_all(g_sender->headers);
	curl_easy_cleanup(g_sender->curl);
}

void *sender_run(void *__unused) {
	while(1) {
		if(!sender_empty()) {
			if(!sender_full()) {
				zlog_debug(g_sender->tag, "Try POST for 1000ms");
				curl_easy_setopt(g_sender->curl, CURLOPT_POSTFIELDS, g_sender->queue[g_sender->head]);
				CURLcode res = curl_easy_perform(g_sender->curl);
				if(res == CURLE_OK) {
					zlog_info(g_sender->tag, "POST success");
					deq_payload();
					continue;
				} else if(res == CURLE_OPERATION_TIMEDOUT) {
					zlog_error(g_sender->tag, "POST failed (timed out)");
				} else {
					zlog_error(g_sender->tag, "POST failed (error code: %d)", res);
					snyo_sleep(NANO/2);
				}
			}
			if(sender_full()) {
				zlog_debug(g_sender->tag, "Write to a file");
				pthread_mutex_lock(&g_sender->mutex);
				FILE *out = fopen("output", "a+");
				while(g_sender->holding>0) {
					fprintf(out, "%s\n", g_sender->queue[g_sender->head]);
					deq_payload();
				}
				fclose(out);
				pthread_mutex_unlock(&g_sender->mutex);
			}
		}
		zlog_debug(g_sender->tag, "Sleep for %lums", NANO/2/1000000);
		snyo_sleep(NANO/2);
	}
	return NULL;
}

int reg_topic(char *payload) {
	curl_easy_setopt(g_sender->curl, CURLOPT_POSTFIELDS, payload);
	return curl_easy_perform(g_sender->curl);
}

bool enq_payload(char *payload) {
	if(sender_full()) return false;
	pthread_mutex_lock(&g_sender->mutex);
	g_sender->queue[g_sender->tail++] = payload;
	g_sender->tail %= MAX_HOLDING;
	g_sender->holding++;
	pthread_mutex_unlock(&g_sender->mutex);
	return true;
}

void deq_payload() {
	free(g_sender->queue[g_sender->head++]);
	g_sender->head %= MAX_HOLDING;
	g_sender->holding--;
}

bool sender_full() {
	return g_sender->holding == MAX_HOLDING;
}

bool sender_empty() {
	return g_sender->holding == 0;
}