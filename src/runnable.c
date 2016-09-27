/**
 * @file runnable.c
 * @author Snyo
 */
#include "runnable.h"
#include "util.h"

#include <stdlib.h>

#include <pthread.h>

#include <zlog.h>

/**
 * return a pointer for success, NULL for failure
 */
runnable_t *new_runnable(double period) {
	if(period <= 0) return NULL;

	runnable_t *app = (runnable_t *)malloc(sizeof(runnable_t));
	if(!app) return NULL;

	app->alive = 0;
	app->period = period * NS_PER_S;

	return app;
}

void delete_runnable(runnable_t *app) {
	if(!app) return;
	free(app);
}

void start_runnable(runnable_t *app) {
	app->alive = 1;
	pthread_create(&app->running_thread, NULL, run, app);
}

void *run(void *_app) {
	runnable_t *app = (runnable_t *)_app;

	while(app->alive) {
		app->job(app);
		snyo_sleep(app->period);
	}

	return NULL;
}