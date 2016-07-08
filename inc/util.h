#ifndef _UTIL_H_
#define _UTIL_H_

void systemMsg(char *header, char *body);
void bytePresent(void *str, int size);
char *append(char *_dst, void *src, int n);

#endif