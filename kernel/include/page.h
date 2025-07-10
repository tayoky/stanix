#ifndef PAGE_H
#define PAGE_H

#include <limits.h> //to get PAGE_SIZE

#define PAGE_ALIGN_DOWN(address) (address/PAGE_SIZE * PAGE_SIZE)
#define PAGE_ALIGN_UP(address)  ((address + PAGE_SIZE -1)/PAGE_SIZE * PAGE_SIZE)
#define PAGE_DIV_UP(address)  ((address + PAGE_SIZE -1)/PAGE_SIZE)

#endif