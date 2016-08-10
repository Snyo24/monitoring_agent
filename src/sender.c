#include "sender.h"
#include "util.h"

#include <stdlib.h>

#include <pthread.h>
#include <zlog.h>
#include <curl/curl.h>

sender_t *new_sender(char *server) {
	zlog_category_t *_tag = zlog_get_category("sender");
    if (!_tag) return NULL;
    zlog_debug(_tag, "Allocate sender");
	sender_t *_sender = (sender_t *)malloc(sizeof(sender_t));
	if(!_sender) return NULL;

	_sender->tag = _tag;
	_sender->server = server;

	return _sender;
}

void *run_sender(void *_sender) {
	post_all((sender_t *)_sender);
	return NULL;
}

// TODO exception
void start_sender(sender_t *sender) {
	sender->head = 0;
	sender->tail = 0;
	zlog_debug(sender->tag, "Setup cURL");
	sender->curl = curl_easy_init();
	if(!sender->curl) {
		zlog_error(sender->tag, "cURL init failed");
		return;
	} else {
		zlog_debug(sender->tag, "Setup header");
		sender->headers = NULL;
		sender->headers = curl_slist_append(sender->headers, "Accept: application/json");
		sender->headers = curl_slist_append(sender->headers, "Content-Type: application/json");

		zlog_debug(sender->tag, "Setup option");
		curl_easy_setopt(sender->curl, CURLOPT_URL, sender->server);
		curl_easy_setopt(sender->curl, CURLOPT_HTTPHEADER, sender->headers);
		curl_easy_setopt(sender->curl, CURLOPT_TIMEOUT_MS, 500);
	}
	sender->tag = zlog_get_category("sender");
	pthread_mutex_init(&sender->sender_mutex, NULL);
	// TODO
	// pthread_create(&sender->running_thread, NULL, run_sender, sender);
}

void delete_sender(sender_t *sender) {
	zlog_debug(sender->tag, "Cleaning up");
	curl_easy_cleanup(sender->curl);
	curl_slist_free_all(sender->headers);
}

int simple_send(sender_t *sender, char *payload) {
	pthread_mutex_lock(&sender->sender_mutex);
	curl_easy_setopt(sender->curl, CURLOPT_POSTFIELDS, payload);
	CURLcode res = curl_easy_perform(sender->curl);
	pthread_mutex_unlock(&sender->sender_mutex);
	return res;
}

void post_all(sender_t *sender) {
	while(1) {
		for(; sender->head!=sender->tail; sender->head=(sender->head+1)%MAX_HOLDING) {
			zlog_debug(sender->tag, "Try Posting");
			curl_easy_setopt(sender->curl, CURLOPT_POSTFIELDS, sender->queue[sender->head]);
			CURLcode res = curl_easy_perform(sender->curl);
			if(res == CURLE_OK) {
				zlog_info(sender->tag, "POST success");
			} else if(res == CURLE_OPERATION_TIMEDOUT) {
				zlog_error(sender->tag, "POST failed (timed out)");
				break;
			} else {
				zlog_error(sender->tag, "POST failed (error code: %d)", res);
				break;
			}
		}

		zlog_debug(sender->tag, "Sleep for %lums", NANO/2/1000000);
		snyo_sleep(NANO/2);
	}
}