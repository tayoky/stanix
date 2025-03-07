#ifndef USERSPACE_H
#define USERSPACE_H

#include "page.h"
#include "paging.h"
#include <stdint.h>

void jump_userspace(void *address,void *stack,int argc,char **argv);

#define USER_STACK_TOP 0x80000000000
#define USER_STACK_SIZE 64 *PAGE_SIZE
#define USER_STACK_BOTTOM USER_STACK_TOP - USER_STACK_SIZE

#define USERSPACE_LIMIT (uintptr_t)0x7FFFFFFFFFFFF

//some macro to check ptr
#define CHECK_PTR_INRANGE(ptr) ((uintptr_t)ptr <= USERSPACE_LIMIT)
#define CHECK_PTR(ptr) CHECK_PTR_INRANGE(ptr) && virt2phys((void *)ptr)


static inline int check_mem(void *ptr,size_t count){
	//this check mem is based on one simple trick
	//if we check an address all the addresses of that page are also valid

	uintptr_t start = PAGE_ALIGN_DOWN((uintptr_t)ptr);
	uintptr_t end   = PAGE_ALIGN_UP((uintptr_t)ptr + count);

	count = end / PAGE_SIZE - start / PAGE_SIZE + 1;

	while(count > 0){
		if(!CHECK_PTR(start)){
			return 0;
		}
		start += PAGE_SIZE;
		count --;
	}
	return 1;
}

static inline int check_str(char *str){
	uintptr_t last_valid_page = 0;

	if(!CHECK_PTR(str)){
		return 0;
	}

	last_valid_page = PAGE_ALIGN_DOWN((uintptr_t)str);

	while(*str){
		str++;
		if(PAGE_ALIGN_DOWN((uintptr_t)str) != last_valid_page){
			//change page
			//need to recheck
			if(!CHECK_PTR(str)){
				return 0;
			}

			last_valid_page = PAGE_ALIGN_DOWN((uintptr_t)str);
		}
	}
	return 1;
}

#define CHECK_MEM(ptr,count) check_mem((void *)ptr,count)
#define CHECK_STR(str) check_str((char *)str)
#define CHECK_STRUCT(struc) CHECK_MEM(struc,sizeof(*struc))

#endif