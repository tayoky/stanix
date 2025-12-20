#ifndef _LIBUTILS_HASHMAP_H
#define _LIBUTILS_HASHMAP_H

#include "vector.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef struct utils_hashmap_entry {
	long key;
	void *element;
} utils_hashmap_entry_t;

typedef struct utils_hasmap {
	utils_vector_t *vectors;
	size_t capacity;
} utils_hashmap_t;

static inline void utils_init_hashmap(utils_hashmap_t *hashmap, size_t capacity) {
	if (hashmap->vectors) return;
	hashmap->vectors = malloc(sizeof(utils_vector_t) * capacity);
	memset(hashmap->vectors, 0, sizeof(utils_vector_t) * capacity);
	hashmap->capacity = capacity;
}

static inline void utils_free_hashmap(utils_hashmap_t *hashmap) {
	if (!hashmap->vectors) return;
	for (size_t i=0; i<hashmap->capacity; i++) {
		utils_free_vector(&hashmap->vectors[i]);
	}
	free(hashmap->vectors);
	hashmap->vectors = NULL;
	hashmap->capacity = 0;
}

static inline size_t utils_hashmap_hash(utils_hashmap_t *hashmap, long key) {
	return (size_t)key % hashmap->capacity;
}

static inline void *utils_hashmap_get(utils_hashmap_t *hashmap, long key) {
	utils_vector_t *vector = &hashmap->vectors[utils_hashmap_hash(hashmap, key)];
	utils_hashmap_entry_t *entries = vector->data;
	if (!entries) return NULL;
	for (size_t i=0; i<vector->count; i++) {
		if (entries[i].key == key) {
			return entries[i].element;
		}
	}
	return NULL;
}

static inline void utils_hashmap_add(utils_hashmap_t *hashmap, long key, const void *element) {
	utils_vector_t *vector = &hashmap->vectors[utils_hashmap_hash(hashmap, key)];
	utils_init_vector(vector, sizeof(utils_hashmap_entry_t));
	utils_hashmap_entry_t *entries = vector->data;
	for (size_t i=0; i<vector->count; i++) {
		if (entries[i].key == key) {
			entries[i].element = (void*)element;
			return;
		}
	}
	utils_hashmap_entry_t entry = {
		.key = key,
		.element = (void*)element,
	};
	utils_vector_push_back(vector, &entry);
}


static inline int utils_hashmap_remove(utils_hashmap_t *hashmap, long key) {
	utils_vector_t *vector = &hashmap->vectors[utils_hashmap_hash(hashmap, key)];
	utils_hashmap_entry_t *entries = vector->data;
	if (!entries) return 0;
	for (size_t i=0; i<vector->count; i++) {
		if (entries[i].key == key) {
			if (i != vector->count - 1) memcpy(&entries[i], &entries[vector->count-1], sizeof(utils_hashmap_entry_t));
			utils_vector_pop_back(vector, NULL);
			return 1;
		}
	}
	return 0;
}

#endif