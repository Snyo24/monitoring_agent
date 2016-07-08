#ifndef _COLLECTOR_H_
#define _COLLECTOR_H_

#include <time.h>

enum payloadType {
	TIME_T,
	INTEGER,
	DOUBLE,
	STRING,
};

enum metricType {
	GAUGE,
	RATE,
};

struct metricValue {
	char *name;
	time_t ts;
	enum metricType type;
	char *payload;
};

void collectMetrics(char *payload);

#endif