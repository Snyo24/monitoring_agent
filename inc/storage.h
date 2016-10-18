/**
 * @file storage.h
 * @author Snyo
 */

#ifndef _STORAGE_H_
#define _STORAGE_H_

#include "runnable.h"
#include "squeue.h"

#define STORAGE_CAPACITY 20

typedef runnable_t storage_t;

extern storage_t storage;

int  storage_init(storage_t *storage);
void storage_fini(storage_t *storage);

void storage_main(void *_storage);

int  storage_full(storage_t *storage);
int  storage_empty(storage_t *storage);
void storage_add(storage_t *storage, void *data);
void storage_drop(storage_t *storage);
char *storage_fetch(storage_t *storage);

#endif
