# makefile include to compile a kernel module

include $(TMAKE_DIR)/stanix.mk

CFLAGS += $(KFLAGS)
CFLAGS += -D__MODULE__=1

PROG = $(MOD)
include $(TMAKE_DIR)/tmake-prog.mk
