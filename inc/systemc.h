#ifndef _SYSTEMC_H_
#define _SYSTEMC_H_

#include "metric.h"

#include <stdlib.h>

void collectMetadata(char *buf);
void timeStamp(void *buf);
void hostname(void *buf);

#endif