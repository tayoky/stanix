#ifndef _LIBUTILS_VECTOR_H
#define _LIBUTILS_VECTOR_H

#include "base.h"

typedef struct utils_vector {
	size_t count;
	size_t capacity;
	size_t element_size;
	void *data;
} utils_vector_t;

static inline void utils_init_vector(utils_vector_t *vector, size_t element_size){
	if (vector->data) return;
	vector->data = malloc(element_size);
	vector->capacity = 1;
	vector->count = 0;
	vector->element_size = element_size;
}

static inline void utils_free_vector(utils_vector_t *vector){
	if (!vector->data) return;
	free(vector->data);
	vector->data = NULL;
	vector->count = 0;
}

static inline void *utils_vectot_at(utils_vector_t *vector, size_t index){
	return (index < vector->count) ? (char*)vector->data + index * vector->element_size : NULL;
}

static inline void utils_vector_push_back(utils_vector_t *vector, const void *element){
	if (vector->count == vector->capacity) {
		vector->capacity *= 2;
		vector->data = realloc(vector->data, vector->capacity * vector->element_size);
	}

	void *ptr = (char*)vector->data + ((vector->count++)  * vector->element_size);
	memcpy(ptr, element, vector->element_size);
}

static inline int utils_vector_pop_back(utils_vector_t *vector, void *element){
	if (vector->count == 0) return -1;
		
	void *ptr = (char*)vector->data + ((--vector->count) * vector->element_size);
	if (element) memcpy(element, ptr, vector->element_size);
	return 0;
}
#endif
