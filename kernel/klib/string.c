#include <kernel/string.h>
#include <stdint.h>
#include <kernel/kheap.h>

char *strdup(const char *str){
	size_t str_size = strlen(str) + 1;
	char *new_str = kmalloc(str_size);
	return strcpy(new_str,str);
}
