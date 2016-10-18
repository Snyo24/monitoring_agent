/**
 * @file shash.c
 * @brief Custom hash
 */

#include "shash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void shash_elem_fini_rec(shash_elem_t *elem);
static void double_up(shash_t *shash);
static shash_elem_t *_shash_search(shash_t *shash, const char *key);
static unsigned long hash_value(const char *str);

int shash_init(shash_t *shash) {
	if(!shash)
		return -1;
	shash->table = (shash_elem_t **)malloc(INITIAL_SIZE*sizeof(shash_elem_t *));
	if(!shash->table) {
		free(shash);
		return -1;
	}
	shash->size = INITIAL_SIZE;
	shash->chaining = 0;
	memset(shash->table, 0, INITIAL_SIZE*sizeof(shash_elem_t *));
	return 0;
}

void shash_insert(shash_t *shash, const char *key, void *item) {
	shash_elem_t *elem = _shash_search(shash, key);
	if(!elem) { // If not exist, create it.
		elem = (shash_elem_t *)malloc(sizeof(shash_elem_t));
		if(!elem) return;
		elem->key = strdup(key);
		elem->hash_value = hash_value(key);

		int idx = (elem->hash_value)%(shash->size);
		if(shash->table[idx]) shash->chaining++;
		elem->next = shash->table[idx];
		shash->table[idx] = elem;
		elem->item = item;
	} else { // Overwriting is not allowed
		return;
	}

	if(shash->chaining > (shash->size >> 1)+(shash->size >> 2))
		double_up(shash);
}

void *shash_fetch(shash_t *shash, const char *key) {
	shash_elem_t *elem = _shash_search(shash, key);
	if(!elem)
		return NULL;
	return elem->item;
}

void shash_fini(shash_t *shash) {
	for(int i=0; i<shash->size; ++i) 
		shash_elem_fini_rec(shash->table[i]);
	free(shash->table);
}

/**
 * Declared only in the source file
 */
unsigned long hash_value(const char *str) {
	unsigned long shash = 5381;
	int c;
	while ((c = *str++))
		shash = ((shash << 5) + shash) + c; // djb2
	return shash;
}

void double_up(shash_t *shash) {
	int old_size = shash->size;
	int new_size = (shash->size<<1) + 1;
	shash_elem_t **old_table = shash->table;
	shash_elem_t **new_table = (shash_elem_t **)malloc(new_size*sizeof(shash_elem_t *));
	if(!new_table) return;
	memset(new_table, 0, new_size*sizeof(shash_elem_t *));

	for(int i=0; i<old_size; ++i) {
		shash_elem_t *prev = NULL;
		shash_elem_t *curr = old_table[i];
		for(;;) {
			if(prev) {
				int idx = prev->hash_value%new_size;
				prev->next = new_table[idx];
				new_table[idx] = prev;
			}
			prev = curr;
			if(!(curr=curr->next)) break;
		}
	}
	free(old_table);

	shash->size = new_size;
	shash->chaining = 0;
	shash->table = new_table;
}

void shash_elem_fini_rec(shash_elem_t *elem) {
	if(!elem) {
		return;
	}
	shash_elem_fini_rec(elem->next);
}

shash_elem_t *_shash_search(shash_t *shash, const char *key) {
	if(!key)
		return NULL;
	unsigned long hV = hash_value(key);
	int i = hV%shash->size;
	shash_elem_t *candidate = NULL;

	for(shash_elem_t *e=shash->table[i]; e; e=e->next)
		if(e->hash_value == hV && !strcmp(e->key, key))
			candidate = e;

	return candidate;
}
