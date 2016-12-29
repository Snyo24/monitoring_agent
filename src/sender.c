#include "sender.h"

#include <curl/curl.h>

#include "packet.h"

#define METRIC_URL   "http://52.79.45.223:8080/v1/metrics"
#define REGISTER_URL "http://52.79.45.223:8080/v1/agents"
#define ALERT_URL    "http://52.79.45.223:8080/v1/alert"

#define CONTENT_TYPE "Content-Type: application/vnd.exem.v1+json"

static size_t callback(char *ptr, size_t size, size_t nmemb, void *tag);
static int sender_add_opt(sender_t *sender, const char *url);

static sender_t sender[3];
static struct curl_slist *header;

int sender_init() {
    curl_global_init(CURL_GLOBAL_SSL);

    if(0x00 || !(header = curl_slist_append(header, CONTENT_TYPE))
            || sender_add_opt(&sender[METRIC],   METRIC_URL)   < 0
            || sender_add_opt(&sender[REGISTER], REGISTER_URL) < 0
            || sender_add_opt(&sender[ALERT],    ALERT_URL)    < 0) {
        sender_fini(sender);
        return -1;
    }

    return 0;
}

int sender_add_opt(sender_t *sender, const char *url) {
    return ((sender->curl = curl_easy_init())
            && curl_easy_setopt(sender->curl, CURLOPT_URL, url)           == CURLE_OK
            && curl_easy_setopt(sender->curl, CURLOPT_POST, 1L)           == CURLE_OK
            && curl_easy_setopt(sender->curl, CURLOPT_TIMEOUT, 30)        == CURLE_OK
            && curl_easy_setopt(sender->curl, CURLOPT_NOSIGNAL, 1)        == CURLE_OK
            && curl_easy_setopt(sender->curl, CURLOPT_HTTPHEADER, header) == CURLE_OK
            && curl_easy_setopt(sender->curl, CURLOPT_WRITEFUNCTION, callback) == CURLE_OK) - 1;
}

int sender_fini() {
	curl_slist_free_all(header);
    curl_easy_cleanup(sender[METRIC].curl);
    curl_easy_cleanup(sender[REGISTER].curl);
    curl_easy_cleanup(sender[ALERT].curl);
    return 0;
}

int post(packet_t *pkt) {
    pkt->attempt++;
    char *payload = packet_fetch(pkt);
    if(!payload) return -1;

    while(!__sync_bool_compare_and_swap(&sender[pkt->type].spin, 0, 1));
	curl_easy_setopt(sender[pkt->type].curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(sender[pkt->type].curl, CURLOPT_WRITEDATA, pkt);
	CURLcode curl_code = curl_easy_perform(sender[pkt->type].curl);
    long status_code;
	curl_easy_getinfo(sender[pkt->type].curl, CURLINFO_RESPONSE_CODE, &status_code);
    sender[pkt->type].spin = 0;

	return (curl_code==CURLE_OK && (status_code==202 || status_code==200)) - 1;
}

size_t callback(char *ptr, size_t size, size_t nmemb, void *_pkt) {
    int code = 0;
    for(int i=0; i<nmemb; i++)
        code = code*10 + (ptr[i]-'0');

    ((packet_t *)_pkt)->error = code;

	return nmemb;
}
