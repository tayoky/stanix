#ifndef KERNEL_ATTRIBUTES_H
#define KERNEL_ATTRIBUTES_H

#if defined(__GNUC__) || defined(__clang__)
#define ATTR_MUST_USE __attribute__((warn_unused_result))
#define ATTR_MALLOC __attribute__((malloc))
#define ATTR_PURE __attribute__((pure))
#define ATTR_NORETURN __attribute__((noreturn))
#define ATTR_RETURN_NONNULL __attribute__((returns_nonnull))
#endif

#ifdef __clang__
#define ATTR_THREAD(x) __attribute__((x))
#else
#define ATTR_THREAD(x)
#endif

#define ATTR_CAPABILITY(name) ATTR_THREAD(capability(name))
#define ATTR_REENTRANT_CAPABILITY ATTR_THREAD(reentrant_capability)
#define ATTR_ACQUIRE(lock) ATTR_THREAD(acquire_capability(lock))
#define ATTR_TRY_ACQUIRE(lock, val) ATTR_THREAD(try_acquire_capability(val, lock))
#define ATTR_RELEASE(lock) ATTR_THREAD(release_capability(lock))
#define ATTR_REQUIRE(...) ATTR_THREAD(requires_capability(__VA_ARGS__))
#define ATTR_ACQUIRE_SHARED(lock) ATTR_THREAD(acquire_shared_capability(lock))
#define ATTR_TRY_ACQUIRE_SHARED(lock, val) ATTR_THREAD(try_acquire_shared_capability(val, lock))
#define ATTR_RELEASE_SHARED(lock) ATTR_THREAD(release_shared_capability(lock))
#define ATTR_REQUIRE_SHARED(...) ATTR_THREAD(requires_shared_capability(__VA_ARGS__))
#define ATTR_GUARDED_BY(...) ATTR_THREAD(guarded_by(__VA_ARGS__))
#define ATTR_ACQUIRED_BEFORE(...) ATTR_THREAD(acquired_before(__VA_ARGS__))
#define ATTR_ACQUIRED_AFTER(...) ATTR_THREAD(acquired_after(__VA_ARGS__))

#endif
