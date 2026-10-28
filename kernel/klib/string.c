#include <kernel/string.h>
#include <stdint.h>
#include <kernel/kheap.h>

char *strdup(const char *str) {
	char *new_str = kmalloc(strlen(str) + 1);
	if (!new_str) return NULL;
	return strcpy(new_str, str);
}
