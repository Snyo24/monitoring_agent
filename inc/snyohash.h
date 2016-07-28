/**
 * @file snyohash.h
 *
 * @brief Custom hash
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
 * The initial size of Hash.
 * The size increases automatically if chaning happens more than half size.
 */

#ifndef _SNYOHASH_H_
#define _SNYOHASH_H_

#define INITIAL_SIZE 31

#include <stddef.h>

typedef struct _hash_elem_info hash_elem_t;
/** _hash_info */
typedef struct _hash_info hash_t;

/** 
 * @breif Hash information
 * @param size Size of hash
 * @param chaining Number of chaning
 * @param table table
 */
struct _hash_info {
	size_t size;
	unsigned int chaining;
	hash_elem_t **table;
};

/** @breif yes */
struct _hash_elem_info {
	char *key;
	unsigned long hash_value;
	void *item;
	hash_elem_t *next;
};

/** @breif yes */
hash_t *new_hash();
/** @breif yes */
void hash_insert(hash_t *hT, char *key, void *item);
void *hash_search(hash_t *hT, char *key);
void delete_hash(hash_t *hash);

size_t hash_to_json(hash_t *hT, char *_buf);

#endif