#ifndef _KERNEL_HASHMAP_H
#define _KERNEL_HASHMAP_H

#include <kernel/vector.h>
#include <kernel/kheap.h>
#include <kernel/string.h>
#include <stdint.h>

typedef struct hashmap_entry {
	long key;
	void *element;
} hashmap_entry_t;

typedef struct hasmap {
	vector_t *vectors;
	size_t capacity;
} hashmap_t;

static inline void init_hashmap(hashmap_t *hashmap, size_t capacity) {
	if (hashmap->vectors) return;
	hashmap->vectors = kmalloc(sizeof(vector_t) * capacity);
	memset(hashmap->vectors, 0, sizeof(vector_t) * capacity);
	hashmap->capacity = capacity;
}

static inline void free_hashmap(hashmap_t *hashmap) {
	if (!hashmap->vectors) return;
	for (size_t i=0; i<hashmap->capacity; i++) {
		free_vector(&hashmap->vectors[i]);
	}
	kfree(hashmap->vectors);
	hashmap->vectors = NULL;
	hashmap->capacity = 0;
}

static inline size_t hashmap_hash(hashmap_t *hashmap, long key) {
	return (size_t)key % hashmap->capacity;
}

static inline void *hashmap_get(hashmap_t *hashmap, long key) {
	vector_t *vector = &hashmap->vectors[hashmap_hash(hashmap, key)];
	hashmap_entry_t *entries = vector->data;
	if (!entries) return NULL;
	for (size_t i=0; i<vector->count; i++) {
		if (entries[i].key == key) {
			return entries[i].element;
		}
	}
	return NULL;
}

static inline void hashmap_add(hashmap_t *hashmap, long key, const void *element) {
	vector_t *vector = &hashmap->vectors[hashmap_hash(hashmap, key)];
	init_vector(vector, sizeof(hashmap_entry_t));
	hashmap_entry_t *entries = vector->data;
	for (size_t i=0; i<vector->count; i++) {
		if (entries[i].key == key) {
			entries[i].element = (void*)element;
			return;
		}
	}
	hashmap_entry_t entry = {
		.key = key,
		.element = (void*)element,
	};
	vector_push_back(vector, &entry);
}


static inline int hashmap_remove(hashmap_t *hashmap, long key) {
	vector_t *vector = &hashmap->vectors[hashmap_hash(hashmap, key)];
	hashmap_entry_t *entries = vector->data;
	if (!entries) return 0;
	for (size_t i=0; i<vector->count; i++) {
		if (entries[i].key == key) {
			if (i != vector->count - 1) memcpy(&entries[i], &entries[vector->count-1], sizeof(hashmap_entry_t));
			vector_pop_back(vector, NULL);
			return 1;
		}
	}
	return 0;
}

static inline void hashmap_foreach(hashmap_t *hashmap, void (*func)(void *element, long key, void *arg), void *arg) {
	for (size_t i=0; i<hashmap->capacity; i++) {
		vector_t *vector = &hashmap->vectors[i];
		hashmap_entry_t *entries = vector->data;
		if (!entries) continue;
		for (size_t j=0; j<vector->count; j++) {
			func(entries[j].element, entries[j].key, arg);
		}
	}
}

#endif