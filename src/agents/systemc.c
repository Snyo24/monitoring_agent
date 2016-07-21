#include "systemc.h"
#include "metric.h"
#include "util.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void tmpInit2(struct metric ***_mList, size_t *k, char *conf) {
	// *k = 2;
	// *_mList = (struct metric **)malloc(*k*sizeof(struct metric *));
	// struct metric **mList = *_mList;
	// for(size_t i=0; i<1; ++i) {
	// 	mList[i] = (struct metric *)malloc(sizeof(struct metric));
	// 	mList[i]->name = "ts";
	// 	mList[i]->type = INTEGER;
	// 	mList[i]->collector = timeStamp;
	// }
	// for(size_t i=1; i<2; ++i) {
	// 	mList[i] = (struct metric *)malloc(sizeof(struct metric));
	// 	mList[i]->name = "hostname";
	// 	mList[i]->type = STRING;
	// 	mList[i]->collector = hostname;
	// }
}

void collectMetadata(char *buf) {
	systemMsg("SYSTEM", "Collecting System metrics");

	// size_t k = 0;
	// struct metric **mList = 0;

	// // Initiating (temporary)
	// tmpInit2(&mList, &k, "NOTHING.txt");

	// // Get metrics
	// buf += putMetrics(buf, mList, k);

	// // Finishing
	// freeMetricList(mList, k);
}

void timeStamp(void *res) {
	time_t now = time(0);
	sprintf(res, "%ld", now);
}

void hostname(void *res) {
	// Error condition
	if(gethostname(res, sizeof(res)))
		((char *)res)[0] = '\0';
}