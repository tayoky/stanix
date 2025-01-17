#ifndef STRING_H
#define STRING_H
#include <stdint.h>
#include <stddef.h>

char *strcpy(char *dest, const char *src);

size_t strlen(const char *str);

void *memset(void *pointer,int value,uint64_t count);

int strcmp(const char *s1, const char *s2);

char *strdup(const char *str);

void *memcpy(void *dest, const void *src,size_t n);
#endif