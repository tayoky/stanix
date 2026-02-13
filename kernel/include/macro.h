#ifndef _KERNEL_MACRO_H
#define _KERNEL_MACRO_H

// a bunch of macro utils

#include <stddef.h>

#define STRINGIFY(a) _STRINGIFY(a)
#define _STRINGIFY(a) #a

#define container_of(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))
#define arraylen(array) (sizeof(array)/sizeof(*(array)))

#endif
