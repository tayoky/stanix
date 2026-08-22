#ifndef KERNEL_CONFIG_H
#define KERNEL_CONFIG_H

// kernel compile time config

// enable poison
// poison with 0xaa for allocate non initalized regions
// poison with 0xdd for freed regions
#define ENABLE_POISON

// enable kernel assertion checks
#define ENABLE_KASSERT

// enable deadlock debugger
// work only with no SMP
#define ENABLE_SPINLOCK_DEBUG

#endif
