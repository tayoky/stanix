#ifndef KERNEL_CONFIG_H
#define KERNEL_CONFIG_H

// kernel compile time config

// enable poison
// poison with 0xaa for allocate non initialized regions
// poison with 0xdd for freed regions
#define ENABLE_POISON

// enable kernel assertion checks
#define ENABLE_KASSERT

// enable deadlock debugger
// work only with no SMP
#define ENABLE_SPINLOCK_DEBUG

// enable the preemption disable leak detector
#define ENABLE_PREEMPT_LEAK_DETECTOR

// enable runtime lock (mutex and rwsem) leak detector
#define ENABLE_LOCK_LEAK_DETECTOR

// enable the printing of debug info on userspace segfault
// this is unsafe and introduce a security vulnerability
#define ENABLE_USER_SEGFAULT_DEBUG

#endif
