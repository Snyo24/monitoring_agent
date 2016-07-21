/*************************************************************
* FILENAME: metric.h
*
* DESCRIPTION :
*     Header file about metric.
*
*
* AUTHOR: Snyo
*
* HISTORY:
*     160704 - first written
*
*/

#ifndef _METRIC_H_
#define _METRIC_H_

#include <stddef.h>

typedef struct _metric_info {
	enum {
		STRING,
		INTEGER,
		DOUBLE,
		BOOLEAN
	} type;
	char *unit;
	union {
		char *str;
		int num_int;
		double num_double;
		unsigned boolean : 1;
	} value;
} metric_t;

metric_t *create_metric(int type, char *unit, void *value);
void destroy_metric(metric_t *m);

size_t metric_to_json(metric_t *m, char *buf);

#endif