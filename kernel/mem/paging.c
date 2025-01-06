#include "paging.h"

void init_PMLT4(uint64_t PMLT4[512]){
	for (uint16_t i = 0; i < 512; i++)
	{
		PMLT4[i] = 0;
	}
	//map the last entry of PMLT4 to itself
	PMLT4[511] = PMLT4;
}

void *virt2phys(void *address){
	uint64_t PMLT4= ((uint64_t)address >> 39) & 0x1FF;
	uint64_t PDP  = ((uint64_t)address >> 30) & 0x1FF;
	uint64_t PD   = ((uint64_t)address >> 21) & 0x1FF;
	uint64_t PT   = ((uint64_t)address >> 12) & 0x1FF;
}

void map_page();