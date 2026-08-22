#ifndef KERNEL_ASSERT_H
#define KERNEL_ASSERT_H

#include <kernel/config.h>

#ifdef ENABLE_KASSERT
#include <kernel/panic.h>
#include <kernel/macro.h>
#define kassert(cond) if (!(cond)) {panic(__FILE__ ":" STRINGIFY(__LINE__) " assert : '" #cond "' failed", NULL);}
#else
#define kassert(cond)
#endif

#endif
