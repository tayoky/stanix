#ifndef _KERNEL_ASSERT_H
#define _KERNEM_ASSERT_H

//#define KASSERT_DEBUG

#ifdef KASSERT_DEBUG
#include <kernel/panic.h>
#define kassert(cond) if (!(cond)) {panic("assert : '" #cond "' failed", NULL);}
#else
#define kassert(cond)
#endif

#endif