#ifndef PAGING_H
#define PAGING_H
#include <stdint.h>

void init_PMLT4(uint64_t PMLT4[512]);
void map_page(uint64_t *PMLT4,uint64_t physical_page,uint64_t virtual_page);
void unmap_page(uint64_t *PMLT4,uint64_t virtual_page);
void map_kernel(uint64_t *PMLT4);
void *virt2phys(void *address);

#ifndef NULL
#define NULL (void *)0
#endif

#endif