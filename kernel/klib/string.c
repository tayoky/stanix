#include "string.h"
#include <stdint.h>

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

//this implementation is from stack overflow : 
//https://stackoverflow.com/questions/34873209/implementation-of-strcmp
int strcmp(const char *s1, const char *s2){
	int i = 0, flag = 0;    
    while (flag == 0) {
        if (s1[i] > s2[i]) {
            flag = 1;
        } else
        if (s1[i] < s2[i]) {
            flag = -1;
        } else {
            i++;
        }
    }
    return flag;
}