/**
 * @file sender.h
 * @author Snyo 
 */

#ifndef _SENDER_H_
#define _SENDER_H_

#include <stdbool.h>

#include <pthread.h>
#include <curl/curl.h>

#define QUEUE_MAX 5
#define QUEUE_EXTRA 66

#define UNSENT_FILE_NAME_MAX 16
#define BACKOFF_EXP 6

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
	char *queue[QUEUE_MAX + QUEUE_EXTRA];

	/* Thread variables */
	pthread_t       running_thread;
	pthread_mutex_t enqmtx;

	/* Logging */
	void *tag;

	/* Sending fail */
	FILE *unsent_fp;
	char _unsent_file_name[UNSENT_FILE_NAME_MAX];
	unsigned backoff :BACKOFF_EXP;
} sender_t;

sender_t g_sender;

void sender_init();
void sender_fini();

bool sender_post(char *payload);
void enq_payload(char *payload);

#endif