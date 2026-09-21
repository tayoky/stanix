# makefile include to build a program

PROG ?= $(PACKAGE)
SRCS ?= $(wildcard *.[cs])
SRCNAMES = $(notdir $(SRCS))
SRCDIRS += $(dir $(SRCS))
OBJS += $(SRCNAMES:%=$(BUILDDIR)/%.o)
CFLAGS := -I ./ $(CFLAGS)

BIN += $(BUILDDIR)/$(PROG)
BINCLEANFILES += $(BUILDDIR)/$(PROG) $(OBJS)
BINMODE ?= 0755
FILESGROUPS += BIN

include $(TMAKE_DIR)/tmake-incs.mk
include $(TMAKE_DIR)/tmake-compile.mk

$(BUILDDIR)/$(PROG) : $(OBJS)
	@mkdir -p "$(@D)"
	@echo "CCLD $(PROG)"
	$(Q)$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
