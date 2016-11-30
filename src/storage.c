/**
 * @file storage.c
 * @author Snyo
 */

#include "storage.h"

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <json/json.h>
#include <zlog.h>

#include "runnable.h"

#define STORAGE_TICK 3
#define zlog_unsent(cat, fmt, ...) zlog(cat,__FILE__,sizeof(__FILE__)-1,__func__,sizeof(__func__)-1,__LINE__, 19,fmt,##__VA_ARGS__)

struct packet_t {
	json_object *data;
};

void storage_lock(storage_t *storage);
void storage_unlock(storage_t *storage);

int storage_init(storage_t *storage) {
	if(!storage) return -1;
	if(runnable_init((runnable_t *)&storage) < 0) return -1;

	storage->tag = zlog_get_category("storage");
	if(!storage->tag);

	DEBUG(zlog_debug(storage->tag, "Initializing"));

	storage->period = STORAGE_TICK;
	storage->job    = storage_main;

	storage->head    = 0;
	storage->tail    = 0;
	storage->holding = 0;
	storage->intlock = 0;
	pthread_mutex_init(&storage->lock, NULL);
    memset(storage->queue, 0, sizeof(storage->queue));

	DEBUG(zlog_debug(storage->tag, "Done"));

	return 0;
}

void storage_fini(storage_t *storage) {
	runnable_fini((runnable_t *)storage);
	pthread_mutex_destroy(&storage->lock);
}

void storage_main(void *_storage) {
	storage_t *storage = (storage_t *)_storage;

	DEBUG(zlog_debug(storage->tag, "Holding %d/%d", storage->holding, CAPACITY));

	if(storage_full(storage)) {
		zlog_warn(storage->tag, "Full storage, store unsent JSON");
		while(!storage_empty(storage)) {
			zlog_unsent(storage->tag, "%s", storage_fetch(storage));
			storage_drop(storage);
		}
        storage->intlock = 0;
	}
}

// Plugin
void storage_add(storage_t *storage, void *data) {
    while(__sync_bool_compare_and_swap(&storage->intlock, 0, 1));

	packet_t *packet = (packet_t *)malloc(sizeof(packet_t));
	packet->data = data;
	storage->queue[storage->tail++] = packet;
	storage->tail %= CAPACITY;
	storage->holding++;

    if(!storage_full(storage))
        storage->intlock = 0;
}

void storage_drop(storage_t *storage) {
    pthread_mutex_lock(&storage->lock);
	json_object_put(storage->queue[storage->head]->data);
	free(storage->queue[storage->head]);
    storage->queue[storage->head++] = 0;
	storage->head %= CAPACITY;
	storage->holding--;
    pthread_mutex_unlock(&storage->lock);
}

// Sender
const char *storage_fetch(storage_t *storage) {
    const char *fetched;
    pthread_mutex_lock(&storage->lock);
    if(!storage->queue[storage->head]) {
        fetched = NULL;
    } else {
        fetched = json_object_to_json_string(storage->queue[storage->head]->data);
    }
    pthread_mutex_unlock(&storage->lock);
	return fetched;
}

int storage_empty(storage_t *storage) {
	return storage->holding == 0;
}

int storage_full(storage_t *storage) {
	return storage->holding == CAPACITY;
}

void storage_lock(storage_t *storage) {
	pthread_mutex_lock(&storage->lock);
}

void storage_unlock(storage_t *storage) {
	pthread_mutex_unlock(&storage->lock);
}
