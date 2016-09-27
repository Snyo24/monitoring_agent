/** @file util.c @author Snyo */

#include "util.h"
#include "shash.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include <sys/stat.h>

#define rtrim(str) 	do { \
						for(int i=strlen(str)-1; \
							i>=0 && (str[i]==' ' || str[i]=='\t'); \
							--i) \
							str[i] = '\0'; \
					} while(0)

timestamp get_timestamp() {
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	return now.tv_sec * NS_PER_S + now.tv_nsec;
}

void snyo_sleep(timestamp ns) {
	if(ns <= 0)
		return;
	struct timespec timeout = {
		ns/NS_PER_S,
		ns%NS_PER_S
	};
	nanosleep(&timeout, NULL);
}

int file_exist(char *filename) {
	struct stat st;
	return !stat(filename, &st);
}

int parse_conf(const char *file, shash_t *map, ...) {
	FILE *fp = fopen(file, "r");
	if(!fp) return -1;

	va_list args;
	va_start(args, map);

	int line_num = 0;
	char line[1000];

	while(fgets(line, 999, fp)) {
		++line_num;
		if(line[0] == '#' || line[0] == '\n') continue;

		char field[100];
		char *value = va_arg(args, char *);

		int ofs = 0;
		while(line[ofs] == ' ' || line[ofs] == '\t') ++ofs;

		if(sscanf(line+ofs, "%[^:]:%*[ \t]%[^\n]\n", field, value) != 2) {
			printf("Syntax error: %s (%s:%d)\n", line, file, line_num);
			va_end(args);
			return -1;
		}
		rtrim(field);
		rtrim(value);

		shash_insert(map, field, value);
	}

	va_end(args);

	return 0;
}