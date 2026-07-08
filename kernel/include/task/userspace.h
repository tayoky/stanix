#ifndef KERNEL_USERSPACE_H
#define KERNEL_USERSPACE_H

#include <kernel/page.h>
#include <kernel/arch.h>
#include <kernel/mmu.h>
#include <stdint.h>
#include <stddef.h>

void jump_userspace(void *address, void *stack, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4);
int safe_copy_to(void *dest, const void *src, size_t count);
int safe_copy_from(void *dest, const void *src, size_t count);

#define safe_copy_auto_from(dest, src) safe_copy_from(dest, src, sizeof(*dest))
#define safe_copy_auto_to(dest, src)   safe_copy_to(dest, src, sizeof(*src))

// some macros to check ptr
#define CHECK_PTR(ptr) ((uintptr_t)ptr <= MEM_USERSPACE_END)
#define CHECK_MEM(ptr, count) CHECK_PTR((uintptr_t)ptr + count)
#define CHECK_STRUCT(struc)   CHECK_PTR((uintptr_t)struc + sizeof(*struc))

#endif
