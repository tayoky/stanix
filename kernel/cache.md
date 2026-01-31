---
title: page cache
---
Stanix's **page cache** is an helper cache called by most filesystem made to do caching.
A `cache_t` (in `<kernel/cache.h>`) represent a page cache (most of the time put in an inode).
The physical page for specific page aligned offset in the cache can be get from `cache_get_page`.

## cache manipulation
A new page cache be initalized using `init_cache` and freed using `free_cache`.
New pages can be cached using `cache_cache` or `cache_cache_async`, flushed to disk using `cache_flush` or `cache_flush_async`.
Pages can be uncached using `cache_uncache` or `cache_uncache_async`.
When a page is uncached it is first flushed to disk and thus uncaching can fail.
Data can be read/write from/to the cache using `cache_read`/`cache_write`.
The cache can be truncated using `cache_truncate` (non blocking).
The cache can be mapped using `cache_mmap`.

## cache operations
Most operations of a cache are async and described in a `cache_ops_t`.
- `read`
  Read the specified amount of pages at an offset.
- `write`
  Write the specified amount of pages at an offset.
For `read` and `write` operations, the implemenation must call `cache_callback` at the end of the operation and call `cache_get_page` to get the physical addresses of the pages to read/write.

## callbacks
Most of the async API of the page cache take a callback called once the operation is finished.
Callback can be executed in any context (including irq handler) and thus cannot be blocking or take too long to execute.
