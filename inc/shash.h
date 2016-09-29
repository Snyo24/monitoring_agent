/**
 * @file shash.h
 *
 * @brief Custom shash
 * @details (CAUTION) There is no delete function.
 * @author Snyo
 *
 * #### History
 * - 160714
 *  + first written
 * - 160725
 *  + modified to work well with the agent structure
 *  + add auto sizing
 *  + description changed
 *
 * @def INITIAL_SIZE
 * The initial size of shash.
 * The size increases automatically.
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
void *shash_search(shash_t *shash, const char *key);
void shash_fini(shash_t *shash);

int shash_to_json(shash_t *shash, char *json);

#endif