/**
 * @file squeue.h
 * @author Snyo
 * @brief Static queue for storing packets
 */

#ifndef _SQUEUE_H_
#define _SQUEUE_H_

#include <pthread.h>

#define CAPACITY 50

typedef struct _squeue {
	int head;
	int tail;
	int holding;
	void *data[CAPACITY];

	pthread_mutex_t lock;
} squeue_t;

int  squeue_init(squeue_t *squeue);
void squeue_fini(squeue_t *squeue);

void senqueue(squeue_t *squeue, void *data);
void *sdequeue(squeue_t *squeue);

int  squeue_empty(squeue_t *squeue);
int  squeue_full(squeue_t *squeue);
void squeue_lock(squeue_t *squeue);
void squeue_unlock(squeue_t *squeue);

#endif