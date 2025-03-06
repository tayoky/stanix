#ifndef USERSPACE_H
#define USERSPACE_H

#include "page.h"
#include "paging.h"

void jump_userspace(void *address,void *stack,int argc,char **argv);

#define USER_STACK_TOP 0x80000000000
#define USER_STACK_SIZE 64 *PAGE_SIZE
#define USER_STACK_BOTTOM USER_STACK_TOP - USER_STACK_SIZE

#define USERSPACE_LIMIT 0x7FFFFFFFFFFFF

//some macro to check ptr
#define CHECK_PTR_INRANGE(ptr) ptr <= USERSPACE_LIMIT
#define CHECK_PTR(ptr) CHECK_PTR_INRANGE(ptr) && virt2phys(ptr)

#define CHECK_MEM(ptr,count) 

#endif