#include "squeue.h"

#include <stdio.h>
#include <pthread.h>

int squeue_init(squeue_t *squeue) {
	squeue->head = 0;
	squeue->tail = 0;
	squeue->holding = 0;
	pthread_mutex_init(&squeue->lock, NULL);
	return 0;
}

void squeue_fini(squeue_t *squeue) {
	pthread_mutex_destroy(&squeue->lock);
}

void senqueue(squeue_t *squeue, void *data) {
	pthread_mutex_lock(&squeue->lock);
	squeue->data[squeue->tail++] = data;
	squeue->tail %= CAPACITY;
	squeue->holding++;
	pthread_mutex_unlock(&squeue->lock);
}

void *sdequeue(squeue_t *squeue) {
	void *head_data = squeue->data[squeue->head++];
	squeue->head %= CAPACITY;
	squeue->holding--;
	return head_data;
}

void squeue_lock(squeue_t *squeue) {
	pthread_mutex_lock(&squeue->lock);
}

void squeue_unlock(squeue_t *squeue) {
	pthread_mutex_unlock(&squeue->lock);
}

int squeue_empty(squeue_t *squeue) {
	return squeue->holding == 0;
}

int squeue_full(squeue_t *squeue) {
	return squeue->holding >= CAPACITY;
}