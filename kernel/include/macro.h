#ifndef KERNEL_MACRO_H
#define KERNEL_MACRO_H

// a bunch of macro utils

#include <stddef.h>
#include <stdint.h>

#define STRINGIFY(a) _STRINGIFY(a)
#define _STRINGIFY(a) #a

#define container_of(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))
#define arraylen(array) (sizeof(array)/sizeof(*(array)))
#define loop_var(type, var) for (type var, _1 = (type)1; _1; _1 = (type)0)

#define IS_ERR(ptr) (((uintptr_t)(ptr)) > (~0xfffUL))
#define ERR2PTR(err) ((void *)(intptr_t)(err))
#define PTR2ERR(ptr) ((int)(intptr_t)(ptr))

#endif
