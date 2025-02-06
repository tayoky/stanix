#ifndef TSS_H
#define TSS_H

#include <stdint.h>

typedef struct {
	uint32_t reserved;
	uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;
	uint64_t bloat[21]; //yeah just some bloat not even used in long mode
}TSS;

void init_tss();

#endif