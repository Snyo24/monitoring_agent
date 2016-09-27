/**
 * @file storage.h
 * @author Snyo
 */

#ifndef _STORAGE_H_
#define _STORAGE_H_

#include "runnable.h"
#include "shash.h"

typedef runnable_t storage_t;
typedef struct _storage_detail {
	int stored;
	//queue;
} storage_detail_t;

int  storage_init(storage_t *storage);
void storage_main(void *_storage);
void storage_fini(storage_t *storage);

#endif