#include "paging.h"

void init_PMLT4(uint64_t PMLT4[512]){
	for (uint16_t i = 0; i < 512; i++)
	{
		PMLT4[i] = 0;
	}
	//map the last entry of PMLT4 to itself
	PMLT4[511] = (uint64_t)PMLT4;
}

void *virt2phys(void *address){
	uint64_t PMLT4i= ((uint64_t)address >> 39) & 0x1FF;
	uint64_t PDPi  = ((uint64_t)address >> 30) & 0x1FF;
	uint64_t PDi   = ((uint64_t)address >> 21) & 0x1FF;
	uint64_t PTi   = ((uint64_t)address >> 12) & 0x1FF;

	uint64_t *PMLT4 = (uint64_t *)0xFFFFFFFFFFFFF000;
	uint64_t *PDP   = (uint64_t *)0xFFFFFFFFFFE00000 + 0x1000 * PDPi;
	uint64_t *PD    = (uint64_t *)0xFFFFFFFFC0000000 + 0x200000 * PDPi + 0x1000 * PDi;
	uint64_t *PT    = (uint64_t *)0xFFFFFF8000000000 + 0x40000000 * PDPi + 0x200000 * PDi + 0x1000 * PTi;
	if(!PMLT4[PMLT4i] & 1){
		return NULL;
	}
	if(!PDP[PDPi] & 1){
		return NULL;
	}
	if(!PD[PDi] & 1){
		return NULL;
	}
	if(!PT[PTi] & 1){
		return NULL;
	}

	return (void *)((PT[PTi] & 0b11111111111111111111111111111111111111000000000000) + ((uint64_t)address & 0xFFF));
}

void map_page();

