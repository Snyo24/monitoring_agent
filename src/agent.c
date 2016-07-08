#include "collector.h"
#include "util.h"

#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

bool alive = true;

void timer(int sig) {
	systemMsg("AGENT", "Collecting Metrics");

	// Initializing
	char *payload;
	payload = (char *)malloc(0);

	// Start collecting
	collectMetrics(payload);

	printf("Result payload:\nsize: %d\n", (int)sizeof(payload));
	bytePresent(payload, sizeof(payload));

	// Finishing
	free(payload);

	alarm(1);
}

int main(int argc, char **argv) {
	signal(SIGALRM, timer);
	raise(SIGALRM);
	while(alive);
	return 0;
}