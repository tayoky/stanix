#ifndef _LIBUTILS_SHASHMAP_H
#define _LIBUTILS_SHASHMAP_H

#include "vector.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct utils_shashmap_entry {
	const char *key;
	void *element;
} utils_shashmap_entry_t;

typedef struct utils_hasmap {
	utils_vector_t *vectors;
	size_t capacity;
} utils_shashmap_t;

static inline void utils_init_shashmap(utils_shashmap_t *shashmap, size_t capacity) {
	if (shashmap->vectors) return;
	shashmap->vectors = malloc(sizeof(utils_vector_t) * capacity);
	memset(shashmap->vectors, 0, sizeof(utils_vector_t) * capacity);
	shashmap->capacity = capacity;
}

static inline void utils_free_shashmap(utils_shashmap_t *shashmap) {
	if (!shashmap->vectors) return;
	for (size_t i=0; i<shashmap->capacity; i++) {
		utils_free_vector(&shashmap->vectors[i]);
	}
	free(shashmap->vectors);
	shashmap->vectors = NULL;
	shashmap->capacity = 0;
}

static inline size_t utils_shashmap_hash(utils_shashmap_t *shashmap, const char *string) {
    const int p = 31;
    const int m = 1e9 + 9;
    long long hash_value = 0;
    long long p_pow = 1;
    while (*string) {
        hash_value = (hash_value + *string * p_pow) % m;
        p_pow = (p_pow * p) % m;
		string++;
    }
    return hash_value % shashmap->capacity;
}

static inline void *utils_shashmap_get(utils_shashmap_t *shashmap, const char *key) {
	utils_vector_t *vector = &shashmap->vectors[utils_shashmap_hash(shashmap, key)];
	utils_shashmap_entry_t *entries = vector->data;
	if (!entries) return NULL;
	for (size_t i=0; i<vector->count; i++) {
		if (!strcmp(entries[i].key, key)) {
			return entries[i].element;
		}
	}
	return NULL;
}

static inline void utils_shashmap_add(utils_shashmap_t *shashmap, const char *key, const void *element) {
	utils_vector_t *vector = &shashmap->vectors[utils_shashmap_hash(shashmap, key)];
	utils_init_vector(vector, sizeof(utils_shashmap_entry_t));
	utils_shashmap_entry_t *entries = vector->data;
	for (size_t i=0; i<vector->count; i++) {
		if (!strcmp(entries[i].key, key)) {
			entries[i].element = (void*)element;
			return;
		}
	}
	utils_shashmap_entry_t entry = {
		.key = strdup(key),
		.element = (void*)element,
	};
	utils_vector_push_back(vector, &entry);
}


static inline int utils_shashmap_remove(utils_shashmap_t *shashmap, const char *key) {
	utils_vector_t *vector = &shashmap->vectors[utils_shashmap_hash(shashmap, key)];
	utils_shashmap_entry_t *entries = vector->data;
	if (!entries) return 0;
	for (size_t i=0; i<vector->count; i++) {
		if (!strcmp(entries[i].key, key)) {
			free(entries[i].key);
			if (i != vector->count - 1) memcpy(&entries[i], &entries[vector->count-1], sizeof(utils_shashmap_entry_t));
			utils_vector_pop_back(vector, NULL);
			return 1;
		}
	}
	return 0;
}


// foreach macro from hell
#define utils_shashmap_foreach(_key, _element, _shashmap) for(size_t i=0; i<(_shashmap)->capacity; i++) \
for (size_t j=0; j<(_shashmap)->vectors[i].count; j++) \
for (const char * _key      = ((utils_shashmap_entry_t*)((_shashmap)->vectors[i].data))[j].key    , _1=(void*)1; _1; _1=NULL)\
for (void *_element = ((utils_shashmap_entry_t*)((_hashmap)->vectors[i].data))[j].element, *_2=(void*)1; _2; _2=NULL)

#endif