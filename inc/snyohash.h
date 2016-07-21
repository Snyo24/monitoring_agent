/*************************************************************
* FILENAME: snyohash.h
*
* DESCRIPTION :
*     Header file about custom hash. It is implemented
*     to handle metric strings. It does shallow copy.
*     (CAUTION) There is no hash_delete function.
*
* EXAMPLE USAGE:
*     hash_t hT;
*     size_t n = 15;
*     hash_init(&hT, n);
*
*     char* key = "snyo";
*     char value[] = "Hashing implemented";
*     hash_insert(&hT, key, value);
*     
*     char *result = (char *)hash_search(&hT, key);
*     if(result)
*         printf("%s\n", result);
*     hash_destroy(&hT);
*     
*     [Result]
*     Hashing implemented
*
* AUTHOR: Snyo
*
* HISTORY:
*     160714 - first written
*
*/

#ifndef _SNYOHASH_H_
#define _SNYOHASH_H_

#include <stddef.h>

typedef struct _hash_info {
	size_t size;
	struct hash_elem **table;
} hash_t;

struct hash_elem {
	char *key;
	unsigned long key_hash;
	void *item;
	struct hash_elem *next;
};

unsigned long hash_value(char *str);

void hash_init(hash_t *hT, size_t size);
void hash_insert(hash_t *hT, char *key, void *item);
void *hash_search(hash_t *hT, char *key);
void hash_destroy(hash_t *hT);

size_t hash_size(hash_t *hT);
size_t hash_to_json(hash_t *hT, char *_buf, size_t (*elem_to_json)(void *, char *));

void _hash_modify(struct hash_elem *hE, void *value, size_t n);
void _hash_elem_free(struct hash_elem *hE);
struct hash_elem *_hash_elem_find(hash_t *hT, char *key);

#endif