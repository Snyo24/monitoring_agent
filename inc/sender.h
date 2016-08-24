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
#define EXTRA_HOLDING 66 // Need extra space for agents number * 2

typedef struct _sender{
	/* Status */
	volatile unsigned alive :1;

	/* CURL */
	CURL   *curl;
	struct curl_slist *headers;

	/* Buffer info */
	int  head;
	int  tail;
	int  holding;
	char *queue[MAX_HOLDING + EXTRA_HOLDING];

	/* Thread variables */
	pthread_t       running_thread;
	pthread_mutex_t enqmtx;

	/* Logging */
	void *tag;

	/* Sending fail */
	FILE *unsent_fp;
	unsigned backoff :6;
} sender_t;

sender_t g_sender;

void sender_init();
void sender_fini();

bool sender_post(char *payload);
void enq_payload(char *payload);

#endif