#include <kernel/print.h>
#include <kernel/panic.h>
#include <stdint.h>

//undefined behaviour handler

typedef struct source_location {
	const char *filename;
	uint32_t line;
	uint32_t column;
} source_location_t;

typedef struct type_descriptor {
	uint16_t type_kind;
	uint16_t type_info;
	char type_name[1];
} type_descriptor_t;

#define TK_INTEGER 0x0000
#define TK_FLOAT   0x0001
#define TK_UNKNOW  0xffff

typedef struct type_mismatch_data {
	source_location_t loc;
	const type_descriptor_t *type;
	unsigned char log_alignment;
	unsigned char type_check_kind;
} type_mismatch_data_t;

typedef struct overflow_data {
	source_location_t loc;
	const type_descriptor_t *type;
} overflow_data_t;

typedef struct shift_out_of_bounds_data {
	source_location_t loc;
	const type_descriptor_t *shifted_type;
	const type_descriptor_t *exponent_type;
} shift_out_of_bounds_data_t;

static void print_loc(const source_location_t *loc) {
	kdebugf("at %s:%u %u\n", loc->filename, loc->line, loc->column);
}

static void print_typename(const type_descriptor_t *type) {
	kprintf("%s", type->type_name);
}

static int type_is_signed(const type_descriptor_t *type) {
	return type->type_kind == TK_INTEGER && (type->type_info & 0x1);
}

static void print_typevalue(const type_descriptor_t *type, uintptr_t value) {
	switch (type->type_kind) {
	case TK_INTEGER:;
		switch (type->type_info) {
		case 0x0006:
			kprintf("%hhu", (unsigned char)value);
			break;
		case 0x0007:
			kprintf("%hhd", (signed char)value);
			break;
		case 0x0008:
			kprintf("%hu", (unsigned short)value);
			break;
		case 0x0009:
			kprintf("%hd", (short)value);
			break;
		case 0x000a:
			kprintf("%u", (unsigned int)value);
			break;
		case 0x000b:
			kprintf("%d", (int)value);
			break;
		case 0x000c:
			kprintf("%llu", (unsigned long long)value);
			break;
		case 0x000d:
			kprintf("%lld", (long long)value);
			break;
		default:
			kprintf("%lx", (unsigned long)value);
			break;
		}
		break;
	case TK_UNKNOW:
	default:
		kprintf("unknow");
		break;
	}
	kprintf("(");
	print_typename(type);
	kprintf(")");
}

#define DEF(name) void name(){\
	kdebugf("%s reached\n",#name);\
	panic(#name,NULL);\
}

void __ubsan_handle_type_mismatch_v1(const type_mismatch_data_t *data, void *pointer) {
	kdebugf("__ubsan_handle_type_mismatch_v1 reached\n");
	print_loc(&data->loc);

	kdebugf("%s of ", data->type_check_kind == 0x01 ? "write" : "read");
	print_typename(data->type);
	kprintf(" on ");

	uintptr_t alignment = 1UL << data->log_alignment;

	if (!pointer) {
		kprintf("null pointer\n");
	} else if ((uintptr_t)pointer & (alignment - 1)) {
		kprintf("misaligned pointer at %p\n", pointer);
	} else {
		kprintf("insufficient object size at %p\n", pointer);
	}

	panic("__ubsan_handle_type_mismatch_v1 reached", NULL);
}

void __ubsan_handle_pointer_overflow(const source_location_t *loc, void *base, void *result) {
	kdebugf("__ubsan_handle_pointer_overflow reached\n");
	print_loc(loc);

	kdebugf("pointer overflow of ");

	if (base == NULL) {
		kprintf("null pointer");
	} else {
		kprintf("%p", base);
	}

	if (base == result) {
		kprintf(" without offset");
	} else {
		kprintf(" with offset");
	}

	kprintf(" resulting in ");

	if (result == NULL) {
		kprintf("null pointer\n");
	} else {
		kprintf("%p\n", result);
	}

	panic("__ubsan_handle_pointer_overflow reached", NULL);
}

static void handle_overflow(const char *type, const char *middle, const overflow_data_t *data, uintptr_t left, uintptr_t right) {
	print_loc(&data->loc);
	kdebugf("%s of ", type);
	print_typevalue(data->type, left);
	kprintf(" %s ", middle);
	print_typevalue(data->type, right);
	kprintf(" cannot be represented in type ");
	print_typename(data->type);
	kprintf("\n");
}

void __ubsan_handle_add_overflow(const overflow_data_t *data, uintptr_t left, uintptr_t right) {
	kdebugf("__ubsan_handle_add_overflow reached\n");
	handle_overflow("addition", "and", data, left, right);
	panic("__ubsan_handle_add_overflow reached", NULL);
}

void __ubsan_handle_sub_overflow(const overflow_data_t *data, uintptr_t left, uintptr_t right) {
	kdebugf("__ubsan_handle_sub_overflow reached\n");
	handle_overflow("subtraction", "by", data, left, right);
	panic("__ubsan_handle_sub_overflow reached", NULL);
}

void __ubsan_handle_mul_overflow(const overflow_data_t *data, uintptr_t left, uintptr_t right) {
	kdebugf("__ubsan_handle_mul_overflow reached\n");
	handle_overflow("multiplication", "by", data, left, right);
	panic("__ubsan_handle_mul_overflow reached", NULL);
}

void __ubsan_handle_divrem_overflow(const overflow_data_t *data, uintptr_t left, uintptr_t right) {
	kdebugf("__ubsan_handle_divrem_overflow reached\n");
	handle_overflow("division", "by", data, left, right);
	panic("__ubsan_handle_divrem_overflow reached", NULL);
}

void __ubsan_handle_negate_overflow(const overflow_data_t *data, uintptr_t value) {
	kdebugf("__ubsan_handle_negate_overflow reached\n");
	print_loc(&data->loc);
	kdebugf("negation of ");
	print_typevalue(data->type, value);
	kprintf(" cannot be represented in type ");
	print_typename(data->type);
	kprintf("\n");

	panic("__ubsan_handle_negate_overflow reached", NULL);
}

void __ubsan_handle_shift_out_of_bounds(const shift_out_of_bounds_data_t *data, uintptr_t shifted, uintptr_t exponent) {
	kdebugf("__ubsan_handle_shift_out_of_bounds reached\n");
	print_loc(&data->loc);
		
	
	kdebugf("shift exponent of ");
	print_typevalue(data->shifted_type, shifted);
	kprintf(" by ");
	print_typevalue(data->exponent_type, exponent);

	if (type_is_signed(data->exponent_type) && (intptr_t)exponent < 0) {
		kprintf(" is negative\n");
	} else {
		kprintf(" is too large\n");
	}

	panic("__ubsan_handle_shift_out_of_bounds reached", NULL);
}

void __ubsan_handle_builtin_unreachable(const source_location_t *loc) {
	kdebugf("__ubsan_handle_builtin_unreachable reached\n");
	print_loc(loc);
	panic("__ubsan_handle_builtin_unreachable reached", NULL);
}

void __ubsan_handle_nonnull_return_v1(const source_location_t *attr_loc, const source_location_t *ret_loc) {
	(void)attr_loc;
	kdebugf("__ubsan_handle_nonnull_return_v1 reached\n");
	print_loc(ret_loc);
	kdebugf("null pointer returned from function declared to never return null");
	
	panic("__ubsan_handle_nonnull_return_v1 reached", NULL);
}

DEF(__ubsan_handle_out_of_bounds)
DEF(__ubsan_handle_function_type_mismatch)
DEF(__ubsan_handle_vla_bound_not_positive)
DEF(__ubsan_handle_load_invalid_value)
