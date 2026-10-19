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
#define ATTR_CLANG(x) __attribute__((x))
#else
#define ATTR_CLANG(x)
#endif

#ifdef __CHECKER__
#define ATTR_CHECKER(x) __attribute__((x))
#else
# define ATTR_CHECKER(x)
#endif

#define ATTR_CAPABILITY(name) ATTR_CLANG(capability(name))
#define ATTR_REENTRANT_CAPABILITY ATTR_CLANG(reentrant_capability)
#define ATTR_ACQUIRE(lock) ATTR_CLANG(acquire_capability(lock)) ATTR_CHECKER(context(lock, 0, 1))
#define ATTR_TRY_ACQUIRE(lock, val) ATTR_CLANG(try_acquire_capability(val, lock))
#define ATTR_RELEASE(lock) ATTR_CLANG(release_capability(lock)) ATTR_CHECKER(context(lock, 1, 0))
#define ATTR_REQUIRE(lock) ATTR_CLANG(requires_capability(lock)) ATTR_CHECKER(context(lock, 1, 1))
#define ATTR_ACQUIRE_SHARED(lock) ATTR_CLANG(acquire_shared_capability(lock)) ATTR_CHECKER(context(lock, 0, 2))
#define ATTR_TRY_ACQUIRE_SHARED(lock, val) ATTR_CLANG(try_acquire_shared_capability(val, lock))
#define ATTR_RELEASE_SHARED(lock) ATTR_CLANG(release_shared_capability(lock)) ATTR_CHECKER(context(lock, 2, 0))
#define ATTR_REQUIRE_SHARED(lock) ATTR_CLANG(requires_shared_capability(lock)) ATTR_CHECKER(context(lock, 2, 2))
#define ATTR_GUARDED_BY(...) ATTR_CLANG(guarded_by(__VA_ARGS__))
#define ATTR_ACQUIRED_BEFORE(...) ATTR_CLANG(acquired_before(__VA_ARGS__))
#define ATTR_ACQUIRED_AFTER(...) ATTR_CLANG(acquired_after(__VA_ARGS__))

#define ATTR_OWNERSHIP_RETURN(...) ATTR_CLANG(ownership_returns(__VA_ARGS__))
#define ATTR_OWNERSHIP_TAKE(...) ATTR_CLANG(ownership_takes(__VA_ARGS__))

#endif
