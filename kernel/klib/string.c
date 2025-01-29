#include "string.h"
#include <stdint.h>
#include "kheap.h"

char *strcpy(char *dest, const char *src){
	size_t index = 0;
	while (src[index]){
		dest[index] = src[index];
		index++;
	}
	dest[index] = '\0';
	return dest;
}

size_t strlen(const char *str){
	size_t index = 0;
	while (str[index]){
		index ++;
	}
	return index;
}

void *memset(void *pointer,int value,uint64_t count){
	uint8_t *ptr = pointer;
	while (count > 0){
		*ptr = value;
		ptr++;
		count--;
	}
	
	return pointer;
}

//this implementation is from reddit :
//https://www.reddit.com/r/C_Programming/comments/8zd888/implementing_my_own_strcmp_have_i_got_it_right/?rdt=53637
int strcmp(const char *str1,const  char *str2) {
	while (*str1 || *str2) 
	{
		if (*str1 > *str2) return 1;
		if (*str2 > *str1) return -1;
		str1++; str2++;
	}
	return 0;
}

void *memcpy(void *dest, const void *src,size_t n){
	while (n > 0){
		*(char *)dest = *(char *)src;
		(char *)src++;
		(char *)dest++;
		n--;
	}
	return dest;
}

char *strdup(const char *str){
	size_t str_size = strlen(str) + 1;
	char *new_str = kmalloc(str_size);
	return strcpy(new_str,str);
}

char *strcat(char * dest, const char * src){
	while(*dest){
		dest++;
	}
	return strcpy(dest,src);
}

int memcmp(const void *buf1,const void *buf2,size_t count){
	while (count > 0){
		if(*(char *)buf1 < *(char *)buf2)return -1;
		if(*(char *)buf1 > *(char *)buf2)return 1;
		(char *)buf1++;
		(char *)buf2++;
		count--;
	}

	return 0;
}