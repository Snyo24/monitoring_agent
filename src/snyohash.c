/**
 * @file snyohash.c
 *
 * @brief Custom hash
 *
 */

#include "snyohash.h"

#include <stdlib.h>
#include <string.h>

/** Following functions are private */
hash_elem_t *new_hash_elem(char *key, void *item);
hash_elem_t *hash_elem_find(hash_t *hT, char *key);
void delete_hash_elem(hash_elem_t *elem);
void double_up(hash_t *hash);
unsigned long hash_value(char *str);

hash_t *new_hash() {
	hash_t *hash = (hash_t *)malloc(sizeof(hash_t));
	if(!hash)
		return NULL;
	hash->table = (hash_elem_t **)malloc(INITIAL_SIZE*sizeof(hash_elem_t *));
	if(!hash->table) {
		free(hash);
		return NULL;
	}
	hash->size = INITIAL_SIZE;
	hash->chaining = 0;
	memset(hash->table, 0, INITIAL_SIZE*sizeof(hash_elem_t *));
	return hash;
}

void hash_insert(hash_t *hash, char *key, void *item) {
	hash_elem_t *elem = hash_elem_find(hash, key);
	if(!elem) { // If not exist, create it.
		elem = new_hash_elem(key, item);

		size_t idx = elem->hash_value%hash->size;
		if(hash->table[idx]) hash->chaining++;
		elem->next = hash->table[idx];
		hash->table[idx] = elem;
	} else {
		elem->item = item; // shallow copy
	}

	if(hash->chaining > (hash->size >> 1)+(hash->size >> 2))
		double_up(hash);
}

void *hash_search(hash_t *hash, char *key) {
	hash_elem_t *elem = hash_elem_find(hash, key);
	if(!elem)
		return NULL;
	return elem->item;
}

void delete_hash(hash_t *hash) {
	for(size_t i=0; i<hash->size; ++i) 
		delete_hash_elem(hash->table[i]);
	free(hash->table);
	free(hash);
}

/**
 * Declared only in the source file
 */
unsigned long hash_value(char *str) {
	unsigned long hash = 5381;
	int c;
	while ((c = *str++))
		hash = ((hash << 5) + hash) + c; // djb2
	return hash;
}

void double_up(hash_t *hash) {
	size_t old_size = hash->size;
	size_t new_size = (hash->size<<1) + 1;
	hash_elem_t **old_table = hash->table;
	hash_elem_t **new_table = (hash_elem_t **)malloc(new_size*sizeof(hash_elem_t *));
	if(!new_table) return;
	memset(new_table, 0, new_size*sizeof(hash_elem_t *));

	for(size_t i=0; i<old_size; ++i) {
		hash_elem_t *prev = NULL;
		for(hash_elem_t *e=old_table[i]; e; prev=e, e=e->next) {
			if(prev) {
				size_t idx = prev->hash_value%new_size;
				prev->next = new_table[idx];
				new_table[idx] = prev;
			}
		}
		if(prev) {
			size_t idx = prev->hash_value%new_size;
			prev->next = new_table[idx];
			new_table[idx] = prev;
		}
	}
	free(old_table);

	hash->size = new_size;
	hash->chaining = 0;
	hash->table = new_table;
}

hash_elem_t *new_hash_elem(char *key, void *item) {
	hash_elem_t *elem = (hash_elem_t *)malloc(sizeof(hash_elem_t));
	if(!elem)
		return NULL;
	elem->key = key; // shallow copy
	elem->item = item; // shallow copy
	elem->hash_value = hash_value(key);
	elem->next = NULL;
	return elem;
}

void delete_hash_elem(hash_elem_t *elem) {
	if(!elem) return;
	delete_hash_elem(elem->next);
	free(elem);
}

hash_elem_t *hash_elem_find(hash_t *hash, char *key) {
	if(!key)
		return NULL;

	unsigned long hV = hash_value(key);
	size_t i = hV%hash->size;
	hash_elem_t *candidate = NULL;

	for(hash_elem_t *e=hash->table[i]; e; e=e->next)
		if(e->hash_value == hV && !strcmp(e->key, key))
			candidate = e;

	return candidate;
}