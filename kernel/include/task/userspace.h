#ifndef USERSPACE_H
#define USERSPACE_H

#include <kernel/page.h>
#include <kernel/arch.h>
#include <kernel/mmu.h>
#include <stdint.h>
#include <stddef.h>

void jump_userspace(void *address,void *stack,uintptr_t arg1,uintptr_t arg2,uintptr_t arg3,uintptr_t arg4);

//some macro to check ptr
#define CHECK_PTR_INRANGE(ptr) ((uintptr_t)ptr <= MEM_USERSPACE_END)
#define CHECK_PTR(ptr) (CHECK_PTR_INRANGE(ptr) && mmu_virt2phys((void *)ptr) != PAGE_INVALID)


int check_mem(void *ptr,size_t count);
int check_str(char *str);

#define CHECK_MEM(ptr,count) check_mem((void *)ptr,count)
#define CHECK_STR(str) check_str((char *)str)
#define CHECK_STRUCT(struc) CHECK_MEM(struc,sizeof(*struc))

#endif