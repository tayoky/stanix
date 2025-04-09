#ifndef SYM_H
#define SYM_H

#include <stdint.h>
#include <stddef.h>

typedef struct sym {
	char *name;
	size_t size;
	uintptr_t value;
} sym_t;


//this array is generated automatcly (see makefile)
extern const sym_t symbols[];
extern const size_t symbols_count;

#endif