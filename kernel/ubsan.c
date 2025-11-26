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

struct type_mismatch_data {
	struct source_location loc;
	const struct type_descriptor *type;
	unsigned char log_alignment;
	unsigned char type_check_kind;
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

static void print_err(uint32_t error){
	kdebugf("error : %s\n",error_strings[error]);
}

static void print_loc(struct source_location loc){
	kdebugf("location : %s:%u %u\n",loc.filename,loc.line,loc.column);
}

#define DEF(name) void name(){\
	kdebugf("%s reached\n",#name);\
	panic(#name,NULL);\
}

void __ubsan_handle_type_mismatch_v1(const struct type_mismatch_data *data,void *pointer){
	kdebugf("__ubsan_handle_type_mismatch_v1 reached\n");
	print_loc(data->loc);
	kdebugf("type : %hx:%hu\n",data->type->type_kind,data->type->type_info);

	uintptr_t alignement = (uintptr_t)1 << data->log_alignment;

	uint32_t error;
	if(!pointer){
		error = ERR_NullPointerUse;
	} else if ((uintptr_t)pointer & (alignement - 1)){
		error = ERR_MisalignedPointerUse;
	} else {
		error = ERR_InsufficientObjectSize;
	}
	print_err(error);
	kdebugf("at %p\n",pointer);

	panic("__ubsan_handle_type_mismatch_v1 reached",NULL);
}
void __ubsan_handle_pointer_overflow(const struct source_location *loc,void *base,void *result){
	print_loc(*loc);

	uint32_t error;
	if(base == NULL && result == NULL){
		error = ERR_NullptrWithOffset;
	} else if (base == NULL && result != NULL){
		error = ERR_NullptrWithNonZeroOffset;
	} else if (base != NULL && result == NULL){
		error = ERR_NullptrAfterNonZeroOffset;
	} else {
		error = ERR_PointerOverflow;
	}

	print_err(error);

	kdebugf("base   : 0x%p\n",base);
	kdebugf("result : 0x%p\n",result);

	panic("__ubsan_handle_pointer_overflow reached",NULL);
}
DEF(__ubsan_handle_add_overflow)
DEF(__ubsan_handle_sub_overflow)
DEF(__ubsan_handle_mul_overflow)
DEF(__ubsan_handle_negate_overflow)
DEF(__ubsan_handle_out_of_bounds)
DEF(__ubsan_handle_shift_out_of_bounds)
DEF(__ubsan_handle_divrem_overflow)
DEF(__ubsan_handle_function_type_mismatch)
DEF(__ubsan_handle_vla_bound_not_positive)
DEF(__ubsan_handle_builtin_unreachable)
