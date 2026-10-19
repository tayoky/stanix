#ifndef KERNEL_ATOMIC_H
#define KERNEL_ATOMIC_H

#include <stdatomic.h>

#ifdef __CHECKER__
// Sparse does not understand the _Atomic xxx syntax
#define ATOMIC(type) type
#else
#define ATOMIC(type) _Atomic(type)
#endif

#endif
