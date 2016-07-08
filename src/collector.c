#include "collector.h"
#include "mysql.h"
#include "util.h"

#include <stdio.h>
#include <string.h>

void collectMetrics(char *payload) {
	systemMsg("COLLECTOR", "Nothing");

	int ofs = 0;
	collect(payload, &ofs);

	time_t now = time(0);
	append(payload, &now, sizeof(now));
}