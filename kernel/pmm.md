---
title: pmm
---
Stanix's **pmm (physical memory manager)** is the lowest level allocator.
Pmm functions and data structures are defined in `<kernel/pmm.h>`.
It allocate and manage the life time of **pages**, a page being a 4KB block (see `PAGE_SIZE`).
New pages can be allocated with `pmm_allocate_page` and freed with `pmm_free_page`.
Newly allocated pages start with a ref count of 1, that can incremented using `pmm_retain` and decreased using `pmm_free_page`.
A page's data can be duplicated using `pmm_dup_page`.
Each physical page (outside MMIO/ACPI regions) have an associed `page_t` that track the ref count and status of a page (is it dirty, in use, ...).
A `page_t` can be accessed from a physical page adress using `pmm_page_info`.
Modifications of the `page_t` should always be done using atomics.
The zero page is a special page always guaranted to be full of zeros.
A new ref to the zero page can be optained using `pmm_get_zero_page`.
The zero page should never be written.
