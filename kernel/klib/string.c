#include <kernel/string.h>
#include <stdint.h>
#include <kernel/kheap.h>

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

int strcmp(const char *str1,const  char *str2) {
	while (*str1 || *str2) 
	{
		if (*str1 != *str2) return *str1 - *str2;
		str1++; str2++;
	}
	return 0;
}

void *memcpy(void *dest, const void *src,size_t n){
	#ifdef x86_64
	asm("rep movsb"
		: : "D" (dest),
		    "S" (src),
		    "c" (n));
	#else
	char *str1 = dest;
	const char *str2 = src;
	while(n > 0){
		*str1 = *str2;
		str1++;
		str2++;
		n--;
	}
	#endif
	return dest;
}

void *memmove(void *dest, const void *src, size_t n){
	if(dest == src){
		return dest;
	}

	if(dest < src){
		return memcpy(dest,src,n);
	}

	#ifdef x86_64
	asm("rep movsq"
		: : "D" (dest),
		    "S" (src),
		    "c" (n));
	#else
	while(n > 0){
		n--;
		((char *)dest)[n] = ((char *)src)[n];
	}
	#endif
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