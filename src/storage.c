/**
 * @file storage.c
 * @author Snyo
 */

#include "storage.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <json/json.h>
#include <zlog.h>

#include "util.h"

#define STORAGE_TICK NS_PER_S/2
#define zlog_unsent(cat, format, ...) \
                   (zlog(cat,__FILE__,sizeof(__FILE__)-1,__func__,sizeof(__func__)-1,__LINE__, \
                    19,format,##__VA_ARGS__))

int storage_init(storage_t *storage) {
	if(runnable_init(storage, STORAGE_TICK) < 0) return -1;

	storage->tag = zlog_get_category("storage");
	if(!storage->tag);

	if(!(storage->spec = malloc(sizeof(squeue_t)))
		|| squeue_init(storage->spec) < 0)
		return -1;

	storage->job = storage_main;

	return 0;
}

void storage_fini(storage_t *storage) {
	squeue_fini(storage->spec);
	free(storage->spec);
}

void storage_main(void *_storage) {
	storage_t *storage = (storage_t *)_storage;
	squeue_t *packets = (squeue_t *)storage->spec;

	zlog_debug(storage->tag, "Holding %d/%d", packets->holding, STORAGE_CAPACITY);
	
	if(storage_full(storage)) {
		zlog_debug(storage->tag, "Store unsent JSON");
		squeue_lock(packets);
		while(!storage_empty(storage)) {
			void *payload = (void *)sdequeue(packets);
			zlog_unsent(storage->tag, "%s\n", json_object_to_json_string(payload));
			json_object_put(payload);
		}
		squeue_unlock(packets);
	}
}

int storage_full(storage_t *storage) {
	squeue_t *packets = (squeue_t *)storage->spec;
	return packets->holding >= STORAGE_CAPACITY;
}

int storage_empty(storage_t *storage) {
	return squeue_empty(storage->spec);
}

void storage_add(storage_t *storage, void *data) {
	return senqueue(storage->spec, data);
}

void storage_drop(storage_t *storage) {
	json_object_put(sdequeue(storage->spec));
}

char *storage_fetch(storage_t *storage) {
	squeue_t *packets = (squeue_t *)storage->spec;
	return (char *)json_object_to_json_string(packets->data[packets->head]);
}
