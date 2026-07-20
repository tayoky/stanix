# makefile include to build a program

SRCS ?= $(wildcard *.[cs])
OBJS += $(SRCS:%=$(BUILDDIR)/%.o)
CFLAGS := -std=c99 -I ./ $(CFLAGS)

all : $(BUILDDIR)/$(PROG)

include $(TMAKE_DIR)/tmake-compile.mk

$(BUILDDIR)/$(PROG) : $(OBJS)
	@mkdir -p "$(@D)"
	@echo "CCLD $(PROG)"
	$(Q)$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

install : all
	@mkdir -p "$(DESTDIR)$(BINDIR)"
	@echo "INSTALL $(PROG)"
	$(Q)cp "$(BUILDDIR)/$(PROG)" "$(DESTDIR)$(BINDIR)/"

uninstall :
	@echo "UNINSTALL $(PROG)"
	$(Q)rm -f "$(DESTDIR)$(BINDIR)/$(PROG)"

clean :
	@echo "CLEAN $(BUILDDIR)"
	$(Q) rm -rf "$(BUILDDIR)"

.PHONY : all install uninstall clean
