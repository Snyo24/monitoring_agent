#include "sender.h"
#include "util.h"

#include <stdlib.h>
#include <stdbool.h>

#include <pthread.h>
#include <zlog.h>
#include <curl/curl.h>

#define SENDER_TICK NS_PER_S/2

#define CONTENT_TYPE "Content-Type: application/vnd.exem.v1+json"

#define UNSENT_BEGIN -1
#define UNSENT_END 2 // TODO must control with configuration file
#define UNSENT_SENDING "log/unsent_sending"
#define UNSENT_FILENO_OFS 10

#define MAX_JSON_BYTE 4096
#define ONE_SUCCESS_TRY_N_UNSENT 3

#define zlog_unsent(cat, format, ...) \
                   (zlog(cat,__FILE__,sizeof(__FILE__)-1,__func__,sizeof(__func__)-1,__LINE__, \
                    255,format,##__VA_ARGS__))

sender_t sender = {
	.alive = 0,

	.headers = NULL,

	.head    = 0,
	.tail    = 0,
	.holding = 0,

	.unsent_sending_fp = NULL,
	._unsent_file_name = "log/unsent",
	.dirty = -1,
	.backoff   = 1
};

int clear_unsent();
void *sender_run(void *_sender);

char *deq_payload();
bool sender_empty();
bool sender_full();

char *unsent_file(int i);
int load_unsent();
void drop_unsent_sending();
void double_backoff();

int sender_init() {
	sender.tag = zlog_get_category("Sender");
    if(!sender.tag) return -1;

	zlog_debug(sender.tag, "Clear old data");
	if(clear_unsent() < 0)
		zlog_warn(sender.tag, "Fail to clear old data");

	zlog_debug(sender.tag, "Initialize cURL");
	if(!(sender.curl = curl_easy_init()) \
	|| !(sender.headers = curl_slist_append(sender.headers, CONTENT_TYPE)) \
	|| curl_easy_setopt(sender.curl, CURLOPT_HTTPHEADER, sender.headers) > 0 \
	|| curl_easy_setopt(sender.curl, CURLOPT_TIMEOUT, 1) > 0) {
		zlog_error(sender.tag, "Fail to setup cURL");
		sender_fini();
		return -1;
	}

	return 0;
}

void sender_start() {
	sender.alive = 1;
	pthread_create(&sender.running_thread, NULL, sender_run, &sender);
}

void sender_set_uri(const char *uri) {
	curl_easy_setopt(sender.curl, CURLOPT_URL, uri);
}

void sender_fini() {
	zlog_debug(sender.tag, "Cleaning up");
	while(!sender_empty())
		free(deq_payload());
	curl_easy_cleanup(sender.curl);
	curl_slist_free_all(sender.headers);
}

void *sender_run(void *_sender) {
	sender_t *sender = (sender_t *)_sender;

	char unsent_end[20];
	snprintf(unsent_end, 20, "log/unsent.%d", UNSENT_END);

	while(sender->alive) {
		zlog_debug(sender->tag, "Sender has %d/%d JSON", sender->holding, QUEUE_MAX+QUEUE_EXTRA);
		if(sender_full()) {
			zlog_debug(sender->tag, "Store unsent JSON");
			pthread_mutex_lock(&sender->enqmtx);
			while(!sender_empty()) {
				char *payload = deq_payload();
				zlog_unsent(sender->tag, "%s", payload);
				free(payload);
			}
			pthread_mutex_unlock(&sender->enqmtx);
		} else if(!sender_empty()) {
			zlog_debug(sender->tag, "Try POST (timeout: 1s)");
			if(!sender_post(sender->queue[sender->head])) {
				zlog_debug(sender->tag, "POST success");
				free(deq_payload());
				sender->backoff = 1;

				if(!sender->unsent_sending_fp) {
					if(load_unsent() < 0) continue;
				} else if(file_exist(unsent_end)) {
					drop_unsent_sending();
					if(load_unsent() < 0) continue;
				}

				snyo_sleep(NS_PER_S);

				zlog_debug(sender->tag, "POST %d unsent JSON", ONE_SUCCESS_TRY_N_UNSENT);
				for(int i=0; i<ONE_SUCCESS_TRY_N_UNSENT; ++i) {
					char buf[MAX_JSON_BYTE];
					if(!fgets(buf, MAX_JSON_BYTE, sender->unsent_sending_fp)) {
						zlog_debug(sender->tag, "fgets() fail");
						drop_unsent_sending();
						break;
					} else if(sender_post(buf) < 0) {
						zlog_debug(sender->tag, "POST unsent fail");
						break;
					}
				}
			} else {
				zlog_debug(sender->tag, "POST fail");
				zlog_debug(sender->tag, "Sleep for %lums", sender->backoff*SENDER_TICK/1000000);
				snyo_sleep(SENDER_TICK*sender->backoff);
				double_backoff();
			}
		} else {
			zlog_debug(sender->tag, "Sleep for %lums", SENDER_TICK/1000000);
			snyo_sleep(SENDER_TICK);
		}
	}
	return NULL;
}

int clear_unsent() {
	int success = 0;
	for(int i=UNSENT_BEGIN; i<UNSENT_END; ++i) {
		char *unsent = unsent_file(i);
		if(file_exist(unsent))
			success |= remove(unsent);
	}
	if(file_exist(UNSENT_SENDING))
		success |= remove(UNSENT_SENDING);

	return success;
}

int sender_post(char *payload) {
	curl_easy_setopt(sender.curl, CURLOPT_POSTFIELDS, payload);
	CURLcode curl_code = curl_easy_perform(sender.curl);
	long status_code;
	curl_easy_getinfo(sender.curl, CURLINFO_RESPONSE_CODE, &status_code);
	zlog_debug(sender.tag, "POST returns curl code(%d) and http_status(%ld)", curl_code, status_code);
	if(status_code == 403) {
		zlog_error(sender.tag, "Check your license");
		sender_fini();
		exit(1);
	}
	return (curl_code == CURLE_OK && status_code == 202) - 1;
}

void enq_payload(char *payload) {
	pthread_mutex_lock(&sender.enqmtx);
	sender.queue[sender.tail++] = payload;
	sender.tail %= (QUEUE_MAX+QUEUE_EXTRA);
	sender.holding++;
	pthread_mutex_unlock(&sender.enqmtx);
}

char *unsent_file(int i) {
	if(sender.dirty != i) {
		if(i >= 0) {
			snprintf(sender._unsent_file_name+UNSENT_FILENO_OFS, 5, ".%i", i);
		} else {
			sender._unsent_file_name[UNSENT_FILENO_OFS] = '\0';
		}
		sender.dirty = i;
	}
	return sender._unsent_file_name;
}

int load_unsent() {
	if(!file_exist(unsent_file(UNSENT_BEGIN)) && !file_exist(unsent_file(0)))
		return -1;
	for(int i=UNSENT_END; i>=UNSENT_BEGIN; --i) {
		char *unsent = unsent_file(i);
		if(file_exist(unsent)) {
			if(rename(unsent, UNSENT_SENDING))
				return -1;
			sender.unsent_sending_fp = fopen(UNSENT_SENDING, "r");
			return (sender.unsent_sending_fp != NULL)-1;
		}
	}
	return -1;
}

void drop_unsent_sending() {
	zlog_debug(sender.tag, "Close and remove unsent_sending");
	fclose(sender.unsent_sending_fp);
	sender.unsent_sending_fp = NULL;
	remove(UNSENT_SENDING);
}

char *deq_payload() {
	char *payload = sender.queue[sender.head++];
	sender.head %= (QUEUE_MAX+QUEUE_EXTRA);
	sender.holding--;
	return payload;
}

bool sender_empty() {
	return sender.holding == 0;
}

bool sender_full() {
	return sender.holding >= QUEUE_MAX;
}

void double_backoff() {
	sender.backoff <<= 1;
	sender.backoff |= !sender.backoff;
}