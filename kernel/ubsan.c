#include <kernel/print.h>
#include <kernel/panic.h>
#include <stdint.h>

//undefined behaviour handler

struct source_location {
	const char *filename;
	uint32_t line;
	uint32_t column;
};

struct type_descriptor {
	uint16_t type_kind;
	uint16_t type_info;
	char type_name[1];
};

#define TK_INTEGER 0x0000
#define TK_FLOAT   0x0001
#define TK_UNKNOW  0xffff

struct type_mismatch_data {
	struct source_location loc;
	const struct type_descriptor *type;
	unsigned char log_alignment;
	unsigned char type_check_kind;
};

struct overflow_data {
	struct source_location loc;
	const struct type_descriptor *type;
};

struct shift_out_of_bounds_data {
	struct source_location loc;
	const struct type_descriptor *shifted_type;
	const struct type_descriptor *exponent_type;
};

//error types
#define ERR_NullPointerUse 1
#define ERR_MisalignedPointerUse 2
#define ERR_InsufficientObjectSize 3
#define ERR_NullptrWithOffset 4
#define ERR_NullptrWithNonZeroOffset 5
#define ERR_NullptrAfterNonZeroOffset 6
#define ERR_PointerOverflow 7

static char *error_strings[] = {
	[ERR_NullPointerUse] = "NullPointerUse",
	[ERR_MisalignedPointerUse] = "MisalignedPointerUse",
	[ERR_InsufficientObjectSize] = "InsufficientObjectSize",
	[ERR_NullptrWithOffset] = "NullptrWithOffset",
	[ERR_NullptrWithNonZeroOffset] = "NullptrWithNonZeroOffset",
	[ERR_NullptrAfterNonZeroOffset] = "NullptrAfterNonZeroOffset",
	[ERR_PointerOverflow] = "PointerOverflow",
};

static void print_err(uint32_t error) {
	kdebugf("error : %s\n", error_strings[error]);
}

static void print_loc(struct source_location loc) {
	kdebugf("location : %s:%u %u\n", loc.filename, loc.line, loc.column);
}

static void print_typename(const struct type_descriptor *type) {
	kprintf("%s", type->type_name);
}

static void print_typevalue(const struct type_descriptor *type, uintptr_t value) {
	switch (type->type_kind) {
	case TK_INTEGER:;
		switch (type->type_info) {
		case 0x0006:
			kprintf("%hhu", value);
			break;
		case 0x0007:
			kprintf("%hhd", value);
			break;
		case 0x0008:
			kprintf("%hu", value);
			break;
		case 0x0009:
			kprintf("%hd", value);
			break;
		case 0x000a:
			kprintf("%u", value);
			break;
		case 0x000b:
			kprintf("%d", value);
			break;
		case 0x000c:
			kprintf("%llu", value);
			break;
		case 0x000d:
			kprintf("%lld", value);
			break;
		default:
			kprintf("%lx", value);
			break;
		}
		break;
	case TK_UNKNOW:
	default:
		kprintf("unknow");
		break;
	}
}

#define DEF(name) void name(){\
	kdebugf("%s reached\n",#name);\
	panic(#name,NULL);\
}

void __ubsan_handle_type_mismatch_v1(const struct type_mismatch_data *data, void *pointer) {
	kdebugf("__ubsan_handle_type_mismatch_v1 reached\n");
	print_loc(data->loc);
	kdebugf("type : ");
	print_typename(data->type);
	kprintf("\n");

	uintptr_t alignement = (uintptr_t)1 << data->log_alignment;

	uint32_t error;
	if (!pointer) {
		error = ERR_NullPointerUse;
	} else if ((uintptr_t)pointer & (alignement - 1)) {
		error = ERR_MisalignedPointerUse;
	} else {
		error = ERR_InsufficientObjectSize;
	}
	print_err(error);
	kdebugf("while %s %p\n", data->type_check_kind == 0x01 ? "writing" : "reading", pointer);

	panic("__ubsan_handle_type_mismatch_v1 reached", NULL);
}
void __ubsan_handle_pointer_overflow(const struct source_location *loc, void *base, void *result) {
	kdebugf("__ubsan_handle_pointer_overflow reached\n");
	print_loc(*loc);

	uint32_t error;
	if (base == NULL && result == NULL) {
		error = ERR_NullptrWithOffset;
	} else if (base == NULL && result != NULL) {
		error = ERR_NullptrWithNonZeroOffset;
	} else if (base != NULL && result == NULL) {
		error = ERR_NullptrAfterNonZeroOffset;
	} else {
		error = ERR_PointerOverflow;
	}

	print_err(error);

	kdebugf("base   : 0x%p\n", base);
	kdebugf("result : 0x%p\n", result);

	panic("__ubsan_handle_pointer_overflow reached", NULL);
}

static void handle_overflow(const char *type, const struct overflow_data *data, uintptr_t left, uintptr_t right) {
	kdebugf("__ubsan_handle_%s_overflow reached\n", type);
	print_loc(data->loc);
	kdebugf("%s overflow between ", type);
	print_typevalue(data->type, left);
	kprintf(" and ");
	print_typevalue(data->type, right);
	kprintf("\n");

	panic("__ubsan_handle_XXX_overflow reached", NULL);
}

void __ubsan_handle_add_overflow(const struct overflow_data *data, uintptr_t left, uintptr_t right) {
	handle_overflow("add", data, left, right);
}

void __ubsan_handle_sub_overflow(const struct overflow_data *data, uintptr_t left, uintptr_t right) {
	handle_overflow("sub", data, left, right);
}

void __ubsan_handle_mul_overflow(const struct overflow_data *data, uintptr_t left, uintptr_t right) {
	handle_overflow("mul", data, left, right);
}

void __ubsan_handle_shift_out_of_bounds(const struct shift_out_of_bounds_data *data, uintptr_t shifted, uintptr_t exponent) {
	kdebugf("__ubsan_handle_shift_out_of_bounds reached\n");
	print_loc(data->loc);

	kdebugf("shift out of bounds ");
	print_typevalue(data->shifted_type, shifted);
	kprintf(" by ");
	print_typevalue(data->exponent_type, exponent);
	kprintf(" bits\n");

	panic("__ubsan_handle_shift_out_of_bounds reached", NULL);
}

DEF(__ubsan_handle_negate_overflow)
DEF(__ubsan_handle_out_of_bounds)
DEF(__ubsan_handle_divrem_overflow)
DEF(__ubsan_handle_function_type_mismatch)
DEF(__ubsan_handle_vla_bound_not_positive)
DEF(__ubsan_handle_builtin_unreachable)
