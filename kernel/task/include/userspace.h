#ifndef USERSPACE_H
#define USERSPACE_H

#include <kernel/page.h>
#include <kernel/paging.h>
#include <kernel/arch.h>
#include <stdint.h>
#include <stddef.h>

void jump_userspace(void *address,void *stack,uintptr_t arg1,uintptr_t arg2,uintptr_t arg3,uintptr_t arg4);
void load_context(fault_frame *context);

//some macro to check ptr
#define CHECK_PTR_INRANGE(ptr) ((uintptr_t)ptr <= USERSPACE_LIMIT)
#define CHECK_PTR(ptr) (CHECK_PTR_INRANGE(ptr) && virt2phys((void *)ptr))


int check_mem(void *ptr,size_t count);
int check_str(char *str);

#define CHECK_MEM(ptr,count) check_mem((void *)ptr,count)
#define CHECK_STR(str) check_str((char *)str)
#define CHECK_STRUCT(struc) CHECK_MEM(struc,sizeof(*struc))

#endif