/**
 * @file snyohash.h
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

#ifndef _snyohash_H_
#define _snyohash_H_

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
	enum item_t {
		ELEM_BOOLEAN = -5,
		ELEM_INTEGER = -4,
		ELEM_DOUBLE = -3,
		ELEM_PTR = -2, // This type of elem will be shallow copied
		ELEM_STRING = -1
	} item_type;

	/* Chaning */
	shash_elem_t *next;
};

shash_t *shash_init();
void shash_insert(shash_t *shash, char *key, void *item, enum item_t type);
void *shash_search(shash_t *shash, char *key);
void shash_fini(shash_t *shash);

int shash_to_json(shash_t *shash, char *json);

#endif