#ifndef _LIBUTILS_HASHMAP_H
#define _LIBUTILS_HASHMAP_H

#include "vector.h"
#include <stdint.h>

typedef struct utils_hashmap_entry {
	void *key;
	void *element;
} utils_hashmap_entry_t;

typedef int (*utils_hashmap_cmp_func_t)(const void *key1, const void *key2);

typedef struct utils_hasmap {
	utils_vector_t vector;
	utils_hashmap_cmp_func_t compare;
} utils_hashmap_t;

static inline void utils_init_hashmap(utils_hashmap_t *hashmap, utils_hashmap_cmp_func_t compare) {
	utils_init_vector(&hashmap->vector, sizeof(utils_hashmap_entry_t));
	hashmap->compare = compare;
}

static inline void utils_free_hashmap(utils_hashmap_t *hashmap) {
	utils_free_vector(&hashmap->vector);
}

static inline int utils_integer_hashmap(const void *key1, const void *key2) {
	uintptr_t i1 = (uintptr_t)key1;
	uintptr_t i2 = (uintptr_t)key2;
	return i1 - i2;
}

static inline utils_hashmap_entry_t *utils_hashmap_get_entry(utils_hashmap_t *hashmap, const void *key) {
	if (hashmap->vector.count == 0) return NULL;
	size_t start = 0;
	size_t end   = hashmap->vector.count;
	utils_hashmap_entry_t *entries = hashmap->vector.data;

	while(start <= end){
		int i = (end + start)/2;
		int cmp = hashmap->compare(key, entries[i].key);
		if(cmp == 0)return &entries[i];
		if(cmp > 0){
			start = i + 1;
		} else {
			if(i == 0)return NULL;
			end   = i - 1;
		}
	}
	return NULL;
}

static inline void *utils_hashmap_get(utils_hashmap_t *hashmap, const void *key) {
	utils_hashmap_entry_t *entry = utils_hashmap_get_entry(hashmap, key);
	return entry ? entry->element : NULL;
}

static inline void utils_hashmap_add(utils_hashmap_t *hashmap, const void *key, const void *element) {

}

#endif