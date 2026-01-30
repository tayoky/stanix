---
title: kernel heap
---
Stanix's **kernel heap (kheap)** manage most allocations done by the kernel.

Allocations can be done using `kmalloc`, freed using `kfree` and modified using `krealloc`.
