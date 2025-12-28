#ifndef _LIBUTILS_BASE_H
#define _LIBUTILS_BASE_H

#if defined(__KERNEL__) || defined(MODULE)
#include <kernel/kheap.h>
#include <kernel/string.h>
#define malloc kmalloc
#define free kfree
#define realloc krealloc
#else
#include <stdlib.h>
#include <string.h>
#endif

#endif