#ifndef KERNEL_ALLOC_FLAGS_H
#define KERNEL_ALLOC_FLAGS_H

#define ALLOC_FLAG_ATOMIC    0x1 // do not sleep nor evict
#define ALLOC_FLAG_OOM       0x2 // trigger OOM if cannot allocate memory
#define ALLOC_FLAG_EMERGENCY 0x4 // use emegency pages

#endif
