#ifndef KERNEL_ENDIAN_H
#define KERNEL_ENDIAN_H

#include <kernel/string.h>
#include <stdint.h>

#define ENDIAN_DEFINE(type, uint) \
typedef struct le_ ## type { \
	uint8_t data[sizeof(type)]; \
} __attribute__((packed)) le_  ## type ## _t; \
\
static inline le_ ## type ## _t type ## _to_le_ ## type(type ## _t data) { \
	le_ ## type ## _t le_ ## type = {0};\
	for (int i = 0; i < sizeof(le_ ##type.data); i++) {\
		le_ ## type.data[i] = (uint8_t)(data >> (i * 8));\
	}\
	return le_ ## type;\
} \
\
static inline type ## _t le_ ## type ## _to_ ## type(le_ ## type ## _t *le_ ## type) { \
	uint ## _t data = 0; \
	for (int i = 0; i < sizeof(le_ ## type->data); i++) {\
		data |= (uint ##_t)le_ ## type->data[i] << (i * 8);\
	}\
	return (type ## _t)data; \
} \
\
typedef struct be_ ## type { \
	uint8_t data[sizeof(type)]; \
} __attribute__((packed)) be ## type ## _t; \
\
static inline be_ ## type ## _t type ## _to_be_ ## type(type ## _t data) { \
	be_ ## type ## _t be_ ## type = {0};\
	for (int i = 0; i < sizeof(be_ ##type.data); i++) {\
		be_ ## type.data[i] = (uint8_t)(data >> (sizeof(be_ ## type.data) - 1 - i) * 8);\
	}\
	return be_ ## type;\
} \
\
static inline type ## _t be_ ## type ## _to_ ## type(be_ ## type ## _t *be_ ## type) { \
	uint ## _t data = 0; \
	for (int i = 0; i < sizeof(be_ ## type->data); i++) {\
		data |= (uint ##_t)be_ ## type->data[i] << ((sizeof(be_ ## type->data) - 1 - i) * 8);\
	}\
	return (type ## _t)data; \
}

ENDIAN_DEFINE(int16,  uint16)
ENDIAN_DEFINE(uint16, uint16)
ENDIAN_DEFINE(int32,  uint32)
ENDIAN_DEFINE(uint32, uint32)
ENDIAN_DEFINE(int64,  uint64)
ENDIAN_DEFINE(uint64, uint64)

#endif
