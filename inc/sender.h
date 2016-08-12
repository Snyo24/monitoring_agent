/**
 * @file sender.h
 * @author Snyo 
 */

#ifndef _SENDER_H_
#define _SENDER_H_

#include <stdbool.h>

#include <pthread.h>
#include <curl/curl.h>

#define MAX_HOLDING 3 
#define MAX_LENGTH 2048

typedef struct _sender{
	/* Buffer info */
	int  head;
	int  tail;
	int  holding;
	char *queue[MAX_HOLDING];

	/* CURL */
	CURL   *curl;
	struct curl_slist *headers;

	/* Thread variables */
	pthread_t       running_thread;
	pthread_mutex_t mutex;

	/* Logging */
	void *tag;
} sender_t;

sender_t *g_sender;

void sender_init(const char *server);
void sender_fini();

void *sender_run(void *_sender);
int reg_topic(char *payload);
bool enq_payload(char *payload);
void deq_payload();

#endif