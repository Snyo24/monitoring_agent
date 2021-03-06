/**
 * @file routine.c
 * @author Snyo
 */
#include "routine.h"

#include <time.h>
#include <pthread.h>

#include <zlog.h>

#include "util.h"

int routine_init(routine_t *r) {
	if(!r) return -1;

	r->alive = 0;

    r->tag  = NULL;
    r->tick = 3600;
    r->due  = 0;

    r->task = NULL;

    if(0x00 || pthread_mutex_init(&r->ping_me, NULL) < 0
            || pthread_mutex_init(&r->pong_me, NULL) < 0
            || pthread_cond_init(&r->ping, NULL) < 0
            || pthread_cond_init(&r->pong, NULL) < 0) {
        return -1;
    }

	return 0;
}

int routine_fini(routine_t *r) {
    if(0x00 || pthread_mutex_destroy(&r->ping_me) < 0
            || pthread_mutex_destroy(&r->pong_me) < 0
            || pthread_cond_destroy(&r->ping) < 0
            || pthread_cond_destroy(&r->pong) < 0)
        return -1;

	return pthread_join(r->running_thread, NULL);
}

int routine_sync(routine_t *r) {
    epoch_t then = epoch_time() + MSPS / 20;
    struct timespec timeout = {then/MSPS, then%MSPS*NSPMS};
    return (pthread_cond_timedwait(&r->pong, &r->pong_me, &timeout)==0) - 1;
}

int routine_ping(routine_t *r) {
    struct timespec timeout = {0, 0};
    if(pthread_mutex_timedlock(&r->ping_me, &timeout) != 0)
        return -1;
    pthread_cond_signal(&r->ping);
    pthread_mutex_unlock(&r->ping_me);

    return routine_sync(r);
}

int routine_start(routine_t *r) {
    if(!r) return -1;
    DEBUG(if(r->tag) zlog_debug(r->tag, "Start"));
    
	r->alive = 1;
    r->due = 0;
    
	pthread_create(&r->running_thread, NULL, (void *)(void *)routine_main, r);

    if(routine_sync(r) < 0)
        return -1;

    pthread_mutex_lock(&r->ping_me);
    pthread_mutex_unlock(&r->ping_me);

    return 0;
}

int routine_stop(routine_t *r) {
    DEBUG(if(r->tag) zlog_debug(r->tag, "Stop"));

    r->alive = 0;

    pthread_cancel(r->running_thread);
    pthread_mutex_unlock(&r->ping_me);
    pthread_mutex_unlock(&r->pong_me);

    return 0;
}

int routine_restart(routine_t *r) {
    DEBUG(if(r->tag) zlog_debug(r->tag, "Restart"));
    return routine_stop(r)==0 && routine_start(r)==0;
}

void *routine_main(routine_t *r) {
    pthread_mutex_lock(&r->pong_me);
    pthread_mutex_lock(&r->ping_me);
    pthread_cond_signal(&r->pong);
    pthread_mutex_unlock(&r->pong_me);

	while(r->alive) {
        pthread_cond_wait(&r->ping, &r->ping_me);

        pthread_mutex_lock(&r->pong_me);
        pthread_cond_signal(&r->pong);
        pthread_mutex_unlock(&r->pong_me);

        epoch_t begin = epoch_time();
		if(r->task) r->task(r);
        r->due = begin + (epoch_t)(r->tick*MSPS);
	}

	return NULL;
}

void routine_change_task(routine_t *r, void *task) {
    r->task = task;
}

int routine_alive(routine_t *r) {
    return r->alive;
}

int routine_overdue(routine_t *r) {
    return r->due <= epoch_time();
}
