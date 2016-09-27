/** @file storage.c @author Snyo */

#include "storage.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

#include <pthread.h>

#include <zlog.h>

int storage_init(storage_t *storage) {
	storage->tag = zlog_get_category("Storage");
	if(!storage->tag);

	storage->detail = (storage_detail_t *)malloc(sizeof(storage_detail_t));
	if(!storage->detail) return -1;

	storage->alive = 0;
	storage->period = NS_PER_S / 3;
	storage->job = storage_main;

	return 0;
}

void storage_main(void *_storage) {
	storage_t *storage = (storage_t *)_storage;

	for(int i=0; i<1; ++i) {
		zlog_debug(storage->tag, "%lu", storage->period);
	}
}

void storage_fini(storage_t *storage) {
	// TODO
}
