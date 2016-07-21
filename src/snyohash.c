/*************************************************************
* FILENAME: snyohash.c
*
* DESCRIPTION :
*     Source file about custom hash. It is implemented
*     to handle metric strings.
*     (CAUTION) There is no delete function.
*
* AUTHOR: Snyo
*
* HISTORY:
*     160714 - first written
*
*/

#include "snyohash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * hash_value
 * Use djb2 hashing
 */
unsigned long hash_value(char *str) {
	unsigned long hash = 5381;
	int c;
	while ((c = *str++))
		hash = ((hash << 5) + hash) + c;
	return hash;
}

/*
 * hash_init
 * Allocate a hash table.
 * Recommand you to choose the size by following rules.
 * 1) be larger than double of expecting number of element.
 * 2) be a prime number which not divided by expecting number of element.
 */
void hash_init(hash_t *hT, size_t size) {
	hT->size = size*2+1;
	hT->table = (struct hash_elem **)malloc(hT->size*sizeof(struct hash_elem *));
	memset(hT->table, 0, hT->size*sizeof(struct hash_elem *));
}

void hash_insert(hash_t *hT, char *key, void *item) {
	struct hash_elem *hE = _hash_elem_find(hT, key);
	if(!hE) { /* If not exist, create it. */
		unsigned long hV = hash_value(key);
		size_t idx = hV%hash_size(hT);

		hE = (struct hash_elem *)malloc(sizeof(struct hash_elem));

		hE->key = key; // shallow copy
		hE->key_hash = hV;
		hE->next = hT->table[idx];

		hT->table[idx] = hE;
	}
	hE->item = item; // shallow copy
}

void *hash_search(hash_t *hT, char *key) {
	struct hash_elem *hE = _hash_elem_find(hT, key);
	if(!hE)
		return NULL;
	return hE->item;
}

void hash_destroy(hash_t *hT, void (*item_destroy)(void *)) {
	for(size_t i=0; i<hash_size(hT); ++i)
		_hash_elem_free(hT->table[i], item_destroy);
	free(hT->table);
}

size_t hash_size(hash_t *hT) {
	return hT->size;
}

void _hash_elem_free(struct hash_elem *hE, void (*item_destroy)(void *)) {
	if(!hE) return;
	_hash_elem_free(hE->next, item_destroy);
	if(item_destroy)
		item_destroy(hE->item);
	free(hE);
}

struct hash_elem *_hash_elem_find(hash_t *hT, char *key) {
	if(!key)
		return NULL;

	unsigned long hV = hash_value(key);
	size_t i = hV%hash_size(hT);
	struct hash_elem *candidate = NULL;

	for(struct hash_elem *e=hT->table[i]; e; e=e->next)
		if(e->key_hash == hV && !strcmp(e->key, key))
			candidate = e;

	return candidate;
}

size_t hash_to_json(hash_t *hT, char *_buf, size_t (*elem_to_json)(void *, char *)) {
	char *buf = _buf;

	size_t hS = hash_size(hT);
	struct hash_elem *exist[hS];
	unsigned int hash_elem_count = 0;
	for(size_t i=0; i<hS; ++i) {
		struct hash_elem *hE = hT->table[i];
		while(hE) {
			exist[hash_elem_count++] = hE;
			hE = hE->next;
		}
	}
	sprintf(buf++, "{");
	sprintf(buf++, "\n");
	for(size_t i=0; i<hash_elem_count; ++i) {
		sprintf(buf, "\t\t");
		buf += 2;
		sprintf(buf, "\"%s\":", (char *)(exist[i]->key));
		buf += strlen(buf);
		sprintf(buf++, " ");
		if(elem_to_json)
			buf += elem_to_json(exist[i]->item, buf);
		if(i < hash_elem_count-1)
			sprintf(buf++, ",");
		sprintf(buf++, "\n");
	}
	sprintf(buf++, "\t");
	sprintf(buf++, "}");
	return buf - _buf;
}