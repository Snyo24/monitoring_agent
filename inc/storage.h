/**
 * @file storage.h
 * @author Snyo
 */

#ifndef _STORAGE_H_
#define _STORAGE_H_

#include <pthread.h>

#include "runnable.h"

#define CAPACITY 1000

typedef struct packet_t packet_t;

typedef struct storage_t {
	runnable_t;

	int head;
	int tail;
	int holding;
	packet_t *queue[CAPACITY];

	pthread_mutex_t lock;
} storage_t;

extern storage_t storage;

int  storage_init(storage_t *storage);
void storage_fini(storage_t *storage);

void storage_main(void *_storage);

void storage_add(storage_t *storage, void *data);
void storage_drop(storage_t *storage);
const char *storage_fetch(storage_t *storage);

int  storage_empty(storage_t *storage);
int  storage_full(storage_t *storage);

#endif
