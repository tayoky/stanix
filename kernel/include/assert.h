#ifndef KERNEL_ASSERT_H
#define _KERNEM_ASSERT_H

#define KASSERT_DEBUG

#ifdef KASSERT_DEBUG
#include <kernel/panic.h>
#include <kernel/macro.h>
#define kassert(cond) if (!(cond)) {panic(__FILE__ ":" STRINGIFY(__LINE__) " assert : '" #cond "' failed", NULL);}
#else
#define kassert(cond)
#endif

#endif