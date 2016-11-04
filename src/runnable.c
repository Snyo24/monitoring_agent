/**
 * @file runnable.c
 * @author Snyo
 */
#include "runnable.h"

#include <pthread.h>

#include <zlog.h>
 
#include "util.h"

/**
 * return 0 for success, -1 for failure
 */
int runnable_init(runnable_t *app, timestamp period) {
	if(!app) return -1;

	app->alive = 0;
	app->period = period;

	return 0;
}

void runnable_fini(runnable_t *app) {
	// Nothing to be done
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
