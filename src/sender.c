#include "sender.h"
#include "util.h"

#include <stdlib.h>
#include <stdbool.h>

#include <pthread.h>
#include <zlog.h>
#include <curl/curl.h>

#define SENDER_TICK GIGA/5
#define DNS "http://10.10.200.171:8082/v1/agents"
#define UNSENT_NUMBER 2 // TODO must control with configuration file
#define UNSENT_SENDING "log/unsent_sending"
#define MAX_JSON_BYTE 4096
#define ONE_SUCCESS_TRY_N_UNSENT 3
#define zlog_unsent(cat, format, ...) \
                   (zlog(cat,__FILE__,sizeof(__FILE__)-1,__func__,sizeof(__func__)-1,__LINE__, \
                    255,format,##__VA_ARGS__))

void *sender_run(void *_sender);

char *deq_payload();
bool sender_empty();
bool sender_full();

char *unsent_file(int i);
bool load_unsent();
void drop_unsent_sending();
void double_backoff();

void sender_init() {
	zlog_category_t *_tag = zlog_get_category("Sender");
    if (!_tag) return;

	g_sender.alive = 1;

	/* Clear unsent json files */
	sprintf(g_sender._unsent_file_name, "log/unsent");
	for(int i=UNSENT_NUMBER-1; i>=-1; --i) {
		char *unsent = unsent_file(i);
		if(file_exist(unsent))
			if(remove(unsent))
				zlog_error(_tag, "Fail to remove junk unsent");
	}
	remove(UNSENT_SENDING);

	zlog_debug(_tag, "Setup cURL");
	g_sender.curl = curl_easy_init();
	if(!g_sender.curl) {
		zlog_error(_tag, "Fail to setup cURL");
		exit(0);
	} else {
		zlog_debug(_tag, "Setup cURL header");
		g_sender.headers = NULL;
		// g_sender.headers = curl_slist_append(g_sender.headers, "Accept: application/json");
		g_sender.headers = curl_slist_append(g_sender.headers, "Content-Type: application/vnd.exem.v1+json");

		zlog_debug(_tag, "Setup cURL option");
		curl_easy_setopt(g_sender.curl, CURLOPT_URL, DNS);
		curl_easy_setopt(g_sender.curl, CURLOPT_HTTPHEADER, g_sender.headers);
		curl_easy_setopt(g_sender.curl, CURLOPT_TIMEOUT, 1);
	}

	g_sender.head = 0;
	g_sender.tail = 0;
	g_sender.holding = 0;
	
	g_sender.tag = _tag;

	g_sender.unsent_fp = NULL;
	g_sender.backoff = 1;

	pthread_mutex_init(&g_sender.enqmtx, NULL);
	pthread_create(&g_sender.running_thread, NULL, sender_run, &g_sender);
}

void sender_fini() {
	zlog_debug(g_sender.tag, "Cleaning up");
	while(!sender_empty())
		free(deq_payload());
	curl_slist_free_all(g_sender.headers);
	curl_easy_cleanup(g_sender.curl);
}

void *sender_run(void *_sender) {
	sender_t *sender = (sender_t *)_sender;

	char unsent_max[20];
	snprintf(unsent_max, 20, "log/unsent.%d", UNSENT_NUMBER-1);

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
			if(sender_post(sender->queue[sender->head])) {
				zlog_debug(sender->tag, "POST success");
				free(deq_payload());
				sender->backoff = 1;

				if(!sender->unsent_fp) {
					if(!load_unsent()) continue;
				} else if(file_exist(unsent_max)) {
					drop_unsent_sending();
					if(!load_unsent()) continue;
				}

				snyo_sleep(GIGA);

				zlog_debug(sender->tag, "POST %d unsent JSON", ONE_SUCCESS_TRY_N_UNSENT);
				for(int i=0; i<ONE_SUCCESS_TRY_N_UNSENT; ++i) {
					char buf[MAX_JSON_BYTE];
					if(!fgets(buf, MAX_JSON_BYTE, sender->unsent_fp)) {
						zlog_debug(sender->tag, "fgets() fail");
						drop_unsent_sending();
						break;
					} else if(!sender_post(buf)) {
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

bool sender_post(char *payload) {
	curl_easy_setopt(g_sender.curl, CURLOPT_POSTFIELDS, payload);
	CURLcode curl_code = curl_easy_perform(g_sender.curl);
	long status_code;
	curl_easy_getinfo(g_sender.curl, CURLINFO_RESPONSE_CODE, &status_code);
	zlog_debug(g_sender.tag, "POST returns curl code(%d) and http_status(%ld)", curl_code, status_code);
	return curl_code == CURLE_OK && status_code == 202;
}

void enq_payload(char *payload) {
	pthread_mutex_lock(&g_sender.enqmtx);
	zlog_error(g_sender.tag, "Queuing Idx: %d", g_sender.tail);
	g_sender.queue[g_sender.tail++] = payload;
	g_sender.tail %= (QUEUE_MAX+QUEUE_EXTRA);
	g_sender.holding++;
	pthread_mutex_unlock(&g_sender.enqmtx);
}

char *unsent_file(int i) {
	if(i >= 0) {
		snprintf(g_sender._unsent_file_name+10, 5, ".%i", i);
	} else {
		g_sender._unsent_file_name[10] = '\0';
	}
	return g_sender._unsent_file_name;
}

bool load_unsent() {
	if(!file_exist(unsent_file(0)) && !file_exist(unsent_file(-1)))
		return false;
	for(int i=UNSENT_NUMBER-1; i>=-1; --i) {
		char *unsent = unsent_file(i);
		if(file_exist(unsent)) {
			if(rename(unsent, UNSENT_SENDING))
				return false;
			g_sender.unsent_fp = fopen(UNSENT_SENDING, "r");
			return g_sender.unsent_fp != NULL;
		}
	}
	return false;
}

void drop_unsent_sending() {
	zlog_debug(g_sender.tag, "Close and remove unsent_sending");
	fclose(g_sender.unsent_fp);
	g_sender.unsent_fp = NULL;
	remove(UNSENT_SENDING);
}

char *deq_payload() {
	char *ret = g_sender.queue[g_sender.head++];
	g_sender.head %= (QUEUE_MAX+QUEUE_EXTRA);
	g_sender.holding--;
	return ret;
}

bool sender_empty() {
	return g_sender.holding == 0;
}

bool sender_full() {
	return g_sender.holding >= QUEUE_MAX;
}

void double_backoff() {
	g_sender.backoff <<= 1;
	g_sender.backoff |= !g_sender.backoff;
}