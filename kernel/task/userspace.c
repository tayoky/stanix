#include <kernel/userspace.h>

int check_mem(void *ptr,size_t count){
	//this check mem is based on one simple trick
	//if we check an address all the addresses of that page are also valid

	uintptr_t start = PAGE_ALIGN_DOWN((uintptr_t)ptr);
	uintptr_t end   = PAGE_ALIGN_UP((uintptr_t)ptr + count);

	while(start < end){
		if(!CHECK_PTR(start)){
			return 0;
		}
		start += PAGE_SIZE;
	}
	return 1;
}

int check_str(char *str){
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