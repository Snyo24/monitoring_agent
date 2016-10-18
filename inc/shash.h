/**
 * @file shash.h
 *
 * @brief Hash structure for storing plugins
 * @author Snyo
 */

#ifndef _shash_H_
#define _shash_H_

#define INITIAL_SIZE 15

typedef struct _shash_elem shash_elem_t;
typedef struct _shash shash_t;

struct _shash {
	int size;
	unsigned int chaining;
	shash_elem_t **table;
};

struct _shash_elem {
	/* Key */
	char *key;
	unsigned long hash_value;

	/* Item */
	void *item;

	/* Chaning */
	shash_elem_t *next;
};

int shash_init(shash_t *shash);
void shash_insert(shash_t *shash, const char *key, void *item);
void *shash_fetch(shash_t *shash, const char *key);
void shash_fini(shash_t *shash);

int shash_to_json(shash_t *shash, char *json);

#endif
