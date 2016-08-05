/**
 * @file sender.h
 * @author Snyo 
 */

#ifndef _SENDER_H_
#define _SENDER_H_

#include <stddef.h>

#include <pthread.h>
#include <curl/curl.h>

#define MAX_HOLDING 2 
#define MAX_LENGTH 2048

typedef struct {
	/* Buffer info */
	volatile size_t head;
	volatile size_t tail;
	char *queue[MAX_HOLDING];

	/* CURL */
	CURL *curl;
	struct curl_slist *headers;

	/* Threading */
	pthread_t running_thread;
	pthread_mutex_t sender_mutex;

	/* Logging */
	void *tag;

	/* Server host */
	char *server;
} sender_t;

sender_t *new_sender(char *server);
void start_sender(sender_t *sender);
void post_all(sender_t *sender);
char *get_slot(sender_t *sender);
void register_json(sender_t *sender, char *json);
void delete_sender(sender_t *sender);

#endif