/*************************************************************
* FILENAME: snyohash.h
*
* DESCRIPTION :
*     Header file about custom hash. It does shallow copy.
*     (CAUTION) There is no delete function.
*
* AUTHOR: Snyo
*
* HISTORY:
*     160714 - first written
*     160725 - modified to work well with the agent structure
*            - add auto sizing
*            - description changed
*
*/

#ifndef _SNYOHASH_H_
#define _SNYOHASH_H_

#define INITIAL_SIZE 31

#include <stddef.h>

typedef struct _hash_elem_info hash_elem_t;

typedef struct _hash_info {
	size_t size;
	unsigned int chaining;
	hash_elem_t **table;
} hash_t;

struct _hash_elem_info {
	char *key;
	unsigned long hash_value;
	void *item;
	hash_elem_t *next;
};

hash_t *new_hash();
void hash_insert(hash_t *hT, char *key, void *item);
void *hash_search(hash_t *hT, char *key);
void delete_hash(hash_t *hash);

size_t hash_to_json(hash_t *hT, char *_buf);

#endif