#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	size_t alloc_size = 4096;
	if (argc > 1) {
		alloc_size = strtoul(argv[1], NULL, 0);
	}
	// lets spam memory allocations
	void *volatile ptr;
	for (;;) {
		ptr = malloc(alloc_size);
	}
	(void)ptr;
	return 0;
}