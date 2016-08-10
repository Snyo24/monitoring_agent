/**
 * @file snyohash.c
 *
 * @brief Custom hash
 *
 */

#include "snyohash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/** Following functions are private */
shash_elem_t *new_shash_elem(char *key, void *item, enum item_t type);
shash_elem_t *get_shash_elem_by_key(shash_t *shash, char *key);
void delete_shash_elem_rec(shash_elem_t *elem);
void free_shash_elem(shash_elem_t *elem);
void double_up(shash_t *shash);
unsigned long hash_value(char *str);

shash_t *new_shash() {
	shash_t *shash = (shash_t *)malloc(sizeof(shash_t));
	if(!shash)
		return NULL;
	shash->table = (shash_elem_t **)malloc(INITIAL_SIZE*sizeof(shash_elem_t *));
	if(!shash->table) {
		free(shash);
		return NULL;
	}
	shash->size = INITIAL_SIZE;
	shash->chaining = 0;
	memset(shash->table, 0, INITIAL_SIZE*sizeof(shash_elem_t *));
	return shash;
}

void shash_insert(shash_t *shash, char *key, void *item, enum item_t type) {
	shash_elem_t *elem = get_shash_elem_by_key(shash, key);
	if(!elem) { // If not exist, create it.
		elem = new_shash_elem(key, item, type);

		int idx = elem->hash_value%shash->size;
		if(shash->table[idx]) shash->chaining++;
		elem->next = shash->table[idx];
		shash->table[idx] = elem;
	} else if(elem->item_type == type) {
		switch(type) {
			case ELEM_BOOLEAN:
			*(bool *)elem->item = *(bool *)item; break;
			case ELEM_INTEGER:
			*(int *)elem->item = *(int *)item; break;
			case ELEM_DOUBLE:
			*(double *)elem->item = *(double *)item; break;
			case ELEM_PTR:
			elem->item = item; break;
			case ELEM_STRING:
			free(elem->item);
			elem->item = strdup(item); break;
			default:
			memcpy(elem->item, item, type);
		}
	}

	if(shash->chaining > (shash->size >> 1)+(shash->size >> 2))
		double_up(shash);
}

void *shash_search(shash_t *shash, char *key) {
	shash_elem_t *elem = get_shash_elem_by_key(shash, key);
	if(!elem)
		return NULL;
	return elem->item;
}

void delete_shash(shash_t *shash) {
	printf("2\n");
	for(int i=0; i<shash->size; ++i) 
		delete_shash_elem_rec(shash->table[i]);
	free(shash->table);
	free(shash);
}

int shash_to_json(shash_t *shash, char *_json) {
	int elem_count = 0;
	shash_elem_t *elems[shash->size<<1];
	for(int i=0; i<shash->size; ++i)
		for(shash_elem_t *elem=shash->table[i]; elem; elem=elem->next)
			elems[elem_count++] = elem;

	char *json = _json;
	json += sprintf(json, "{");
	for(int i=0; i<elem_count; ++i) {
		json += sprintf(json, "\"%s\":", elems[i]->key);
		switch(elems[i]->item_type) {
			case ELEM_BOOLEAN:
			json += sprintf(json, "%s", *(bool *)elems[i]->item? "true": "false"); break;
			case ELEM_INTEGER:
			json += sprintf(json, "%d", *(int *)elems[i]->item); break;
			case ELEM_DOUBLE:
			json += sprintf(json, "%f", *(double *)elems[i]->item); break;
			case ELEM_PTR:
			break;
			case ELEM_STRING:
			json += sprintf(json, "\"%s\"", (char *)elems[i]->item); break;
			default:
			json += sprintf(json, "{");
			for(int k=0; k<elems[i]->item_type/sizeof(shash_t *); ++k) {
				json += sprintf(json, "\"%d\":", k);
				json += shash_to_json(((shash_t **)elems[i]->item)[k], json);
				if(k < (elems[i]->item_type)/sizeof(shash_t *)-1)
					json += sprintf(json, ",");
			}
			json += sprintf(json, "}");
		}
		if(i < elem_count-1)
			json += sprintf(json, ",");
	}
	json += sprintf(json, "}");

	return json - _json;
}

/**
 * Declared only in the source file
 */
unsigned long hash_value(char *str) {
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
		for(shash_elem_t *e=old_table[i]; e; prev=e, e=e->next) {
			if(prev) {
				int idx = prev->hash_value%new_size;
				prev->next = new_table[idx];
				new_table[idx] = prev;
			}
		}
		if(prev) {
			int idx = prev->hash_value%new_size;
			prev->next = new_table[idx];
			new_table[idx] = prev;
		}
	}
	free(old_table);

	shash->size = new_size;
	shash->chaining = 0;
	shash->table = new_table;
}

shash_elem_t *new_shash_elem(char *key, void *item, enum item_t type) {
	shash_elem_t *elem = (shash_elem_t *)malloc(sizeof(shash_elem_t));
	if(!elem) return NULL;

	elem->key = strdup(key);
	elem->hash_value = hash_value(key);

	switch(type) {
		case ELEM_BOOLEAN:
		elem->item = malloc(sizeof(bool));
		*(bool *)elem->item = *(bool *)item; break;
		case ELEM_INTEGER:
		elem->item = malloc(sizeof(int));
		*(int *)elem->item = *(int *)item; break;
		case ELEM_DOUBLE:
		elem->item = malloc(sizeof(double));
		*(double *)elem->item = *(double *)item; break;
		case ELEM_PTR:
		elem->item = item; break;
		case ELEM_STRING:
		elem->item = strdup(item); break;
		default:
		elem->item = malloc(type);
		memcpy(elem->item, item, type);
	}
	elem->item_type = type;

	elem->next = NULL;
	return elem;
}

void free_shash_elem(shash_elem_t *elem) {
	if(!elem) return;
	free(elem->key);
	switch(elem->item_type) {
		case ELEM_PTR: break;
		case ELEM_BOOLEAN:
		case ELEM_INTEGER:
		case ELEM_DOUBLE:
		case ELEM_STRING:
		free(elem->item); break;
		default:
		for(int k=0; k<elem->item_type/sizeof(shash_t *); ++k)
			delete_shash(((shash_t **)elem->item)[k]);
		free(elem->item);
	}
	free(elem);
}

void delete_shash_elem_rec(shash_elem_t *elem) {
	if(!elem) return;
	delete_shash_elem_rec(elem->next);
	free_shash_elem(elem);
}

shash_elem_t *get_shash_elem_by_key(shash_t *shash, char *key) {
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