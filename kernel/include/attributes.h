#ifndef KERNEL_ATTRIBUTES_H
#define KERNEL_ATTRIBUTES_H

#define ATTR_MUST_USE __attribute__((warn_unused_result))
#define ATTR_MALLOC __attribute__((malloc))
#define ATTR_PURE __attribute__((pure))
#define ATTR_NORETURN __attribute__((noreturn))
#define ATTR_RETURN_NONNULL __attribute__((returns_nonnull))

#endif
