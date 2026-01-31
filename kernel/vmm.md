---
title: vmm
---
Stanix's **virtual memory manager (vmm)** manage the memory mappings of processes. It support features such as **CoW (Copy on Write)**.

## segments
Each process has a list of `vmm_seg_t`, each `vmm_seg_t` cover a range from it's `start` to `end` fields and must be **page aligned**. If a [`vfs_fd_t`](vfs#files-context) is mapped on a `vmm_seg_t`, the `vmm_seg_t` hold a ref to it until it is unmapped.

## flags and protections
Each segment have flags and protections, they indicate how the mapping should behave.
- `VMM_FLAG_PRIVATE`
  The mapping should not be shared, on `vmm_clone` or `fork`, CoW is used.
- `VMM_FLAG_SHARED`
  The mapping should be shared, if multiples process have the same pages mapped or on `vmm_clone` the pages ate shared.
- `VMM_FLAG_ANONYMOUS`
  The mapping is anonymous, it only exist in memory and is not associed with a file or a device.
- `VMM_FLAG_IO`
  The mapping refere to IO (MMIO, framebuffer, ...), the ref count is not tracked and the pages are not freed (if the driver want to free the page, it must do it itself using [`pmm_set_free_page`](pmm)).

### segments operations
Various operations can be done on segments.
- `vmm_map`
  Create a new mapping in memory and can map files/devices (by calling [`vfs_mmap`](vmm#files-functions)).
- `vmm_unmap`
  Unmap and destroy a mapping and can free the pages used by it (if not marked as `VMM_FLAG_IO`).
- `vmm_clone`
  Clone the mapping in another process (only used by `fork`), and setup CoW from `VMM_FLAG_PRIVATE` segments.
- `vmm_chprot`
  Change the protections of a segment.
