#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <common.h>

struct bitmap {
	size_t size;
	uint8_t data[];
};

bitmap_t *bitmap_create(size_t size) {
	bitmap_t *bitmap = malloc(sizeof(bitmap_t) + (size + 7) / 8);
	if (!bitmap) {
		error("out of memory");
		exit(1);
	}
	memset(bitmap, 0, sizeof(bitmap_t) + (size + 7) / 8);
	bitmap->size = size;
	return bitmap;
}

void bitmap_set(bitmap_t *bitmap, long bit) {
	if (bit < 0 || bit >= bitmap->size) return;
	bitmap->data[bit / 8] |= 1U << (bit % 8);
}

long bitmap_allocate(bitmap_t *bitmap) {
	for (size_t i=0; i<bitmap->size / 8; i++) {
		if (bitmap->data[i] == 0xff) continue;
		for (size_t j=0; j<8; j++) {
			if (bitmap->data[i] & (1U << j)) continue;
			return j + i * 8;
		}
	}
	return -1;
}
