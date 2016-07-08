#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void systemMsg(char *header, char *body) {
	printf("[ %9s ] %s\n", header, body);
}

void bytePresent(void *_ptr, int size) {
	for(int i=0; i<size; ++i)
		printf("%x ", *(unsigned char *)(_ptr+i));
	printf("\n");
}

char *append(char *_ptr, void *src, int n) {
	char *newPtr = (char *)realloc(_ptr, sizeof(_ptr)+n);
	memcpy(_ptr, (char *)src, n);
	return newPtr;
}