#include "metric.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

metric_t *create_metric(int type, char *unit, void *value) {
	metric_t *m = (metric_t *)malloc(sizeof(metric_t));

	m->type = type;
	m->unit = unit; // Shallow copy
	size_t n;
	switch(type) {
		case STRING:
		n = strlen((char *)value)+1; // must end with null value
		m->value.str = (char *)malloc(n);
		memcpy(m->value.str, value, n); // deep copy
		break;
		case INTEGER:
		m->value.num_int = *(int *)value;
		break;
		case DOUBLE:
		m->value.num_double = *(double *)value;
		break;
		case BOOLEAN:
		m->value.boolean = *(unsigned *)value;
		break;
	}
	return m;
}

void destroy_metric(metric_t *m) {
	if(m->type == STRING)
		free(m->value.str);
	free(m);
}

size_t metric_to_json(metric_t *m, char *buf) {
	switch(m->type) {
		case STRING:
		sprintf(buf, "\"%s\"", m->value.str);
		break;
		case INTEGER:
		sprintf(buf, "%d", m->value.num_int);
		break;
		case DOUBLE:
		sprintf(buf, "%.2f", m->value.num_double);
		break;
		case BOOLEAN:
		sprintf(buf, "%s", m->value.boolean? "true": "false");
		break;
	}
	return strlen(buf);
}