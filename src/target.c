/**
 * @file target.c
 * @author Snyo
 */
#include "target.h"

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#include <zlog.h>

#include "metadata.h"
#include "storage.h"
#include "util.h"

int sync(target_t *target);

// return 0 for success, -1 for failure
int target_init(target_t *target) {
    if(!target) return -1;

    /* Tag */
    DEBUG(target->tag = zlog_get_category(target->type));
    DEBUG(if(!target->tag));

    target->packets = 0;

    /* Thread variables */
    if(0x00 || pthread_mutex_init(&target->ping_me, NULL) < 0
            || pthread_mutex_init(&target->pong_me, NULL) < 0
            || pthread_cond_init(&target->pong, NULL) < 0
            || pthread_cond_init(&target->ping, NULL) < 0) {
        zlog_error(target->tag, "INIT THREAD VARS FAIL");
        target_fini(target);
        return -1;
    }

    return 0;
}

int target_fini(target_t *target) {
    if(!target) return -1;
    DEBUG(zlog_info(target->tag, "Finialize"));

    if(target->fini)
        target->fini(target);

    if(0x00 || pthread_mutex_destroy(&target->pong_me) < 0
            || pthread_mutex_destroy(&target->ping_me) < 0
            || pthread_cond_destroy(&target->ping) < 0
            || pthread_cond_destroy(&target->pong) < 0)
        return -1;

    return 0;
}

unsigned alive(target_t *target) {
    return target->alive;
}

unsigned outdated(target_t *target) {
    return target->the_time <= epoch_time();
}

int start(target_t *target) {
    DEBUG(zlog_info(target->tag, "Start"));

    target->alive   = 1;
    target->the_time = 0;

    pthread_mutex_lock(&target->pong_me);
    pthread_create(&target->running_thread, NULL, target_main, target);

    if(sync(target) < 0)
        return -1;

    pthread_mutex_lock(&target->ping_me);
    pthread_mutex_unlock(&target->ping_me);

    return 0;
}

void stop(target_t *target) {
    DEBUG(zlog_info(target->tag, "Stop"));

    target->alive = 0;

    pthread_cancel(target->running_thread);
    pthread_mutex_unlock(&target->pong_me);
    pthread_mutex_unlock(&target->ping_me);
}

void restart(target_t *target) {
    DEBUG(zlog_info(target->tag, "Restart"));
    stop(target);
    start(target);
}

int ping(target_t *target) {
    pthread_mutex_lock(&target->ping_me);
    pthread_cond_signal(&target->ping);
    pthread_mutex_unlock(&target->ping_me);

    return sync(target);
}

int sync(target_t *target) {
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += 1;
    return (pthread_cond_timedwait(&target->pong, &target->pong_me, &timeout)==0) - 1;
}

void *target_main(void *_target) {
    target_t *target = (target_t *)_target;

    pthread_mutex_lock(&target->pong_me);
    pthread_mutex_lock(&target->ping_me);
    pthread_cond_signal(&target->pong);
    pthread_mutex_unlock(&target->pong_me);

    while(target->alive) {
        DEBUG(zlog_debug(target->tag, ".. Ping me"));
        pthread_cond_wait(&target->ping, &target->ping_me);
        DEBUG(zlog_debug(target->tag, ".. Pinged"));

        pthread_mutex_lock(&target->pong_me);
        pthread_cond_signal(&target->pong);
        pthread_mutex_unlock(&target->pong_me);

        packet_t *p = target->packets;
        if(!target->packets) {
            p = alloc_packet();
            target->packets = p;
            p->ofs += sprintf(p->payload+p->ofs, "{\"license\":\"%s\",\"tid\":%llu,\"metrics\":[", license, target->tid);
        }
        if(packet_expire(p)) {
            p->ofs += sprintf(p->payload+p->ofs, "]}");
            p->next = alloc_packet();
            p->ready = 1;
            p = p->next;
        }

        DEBUG(zlog_debug(target->tag, ".. Gather"));

        epoch_t begin = epoch_time();
        p->ofs += sprintf(p->payload+p->ofs, "%s{\"timestamp\":%llu,\"values\":{", packet_new(p)?"":",", begin);
        p->ofs += target->gather(_target, p->payload+p->ofs);
        p->ofs += sprintf(p->payload+p->ofs, "}}");
        target->the_time = begin + (epoch_t)(target->tick*MSPS);

        DEBUG(zlog_debug(target->tag, ".. %d bytes, in %llums", p->ofs, epoch_time()-begin));
    }

    return NULL;
}
