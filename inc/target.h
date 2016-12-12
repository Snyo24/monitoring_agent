/**
 * @file target.h
 * @author Snyo
 */
#ifndef _TARGET_H_
#define _TARGET_H_

#include <pthread.h>

#include "packet.h"
#include "util.h"

typedef struct target_t {
	int index;
	
	/* Thread */
	pthread_t       running_thread;
	pthread_mutex_t ping_me;
	pthread_mutex_t pong_me;
	pthread_cond_t  ping;
	pthread_cond_t  pong;

	/* Status */
	volatile unsigned alive : 1;

    /* Info */
    unsigned long long tid;
	const char *tip;
	const char *type;

    packet_t *packets;

	/* Timing */
	float tick;
    epoch_t the_time;

	/* Polymorphism */
	void *tag;
	int  (*gather)(void *, char *);
	void (*fini)(void *);
} target_t;

int target_init(target_t *target);
int target_fini(target_t *target);

unsigned alive(target_t *target);
unsigned outdated(target_t *target);

int  start(target_t *target);
void stop(target_t *target);
void restart(target_t *target);
int  ping(target_t *target);

void *target_main(void *_target);

#endif
