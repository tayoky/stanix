#ifndef TSS_H
#define TSS_H

#include <stdint.h>

typedef struct {
	uint32_t reserved;
	uint32_t rspl0;
	uint32_t rsph0;
	uint32_t rspl1;
	uint32_t rsph1;
	uint32_t rspl2;
	uint32_t rsph2;
	uint64_t bloat[22]; //yeah just some bloat not even used in long mode
}TSS;

void init_tss();

#endif