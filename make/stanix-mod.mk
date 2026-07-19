# makefile include to compile a kernel module

include $(TMAKE_DIR)/stanix-kflags.mk

CFLAGS += $(KFLAGS)
CFLAGS += -D__MODULE__=1

SRCS ?= $(wildcard *.[cs])
OBJS += $(SRCS:%=$(BUILDDIR)/%.o)
MOD_KO = $(MOD).ko

all : $(BUILDDIR)/$(MOD_KO)

include $(TMAKE_DIR)/tmake-compile.mk

$(BUILDDIR)/$(MOD_KO) : $(OBJS)
	@mkdir -p "$(@D)"
	@echo "CCLD $(MOD_KO)"
	$(Q)$(CC) $(CFLAGS) -r -nostdlib -o $@ $^ $(LDFLAGS)

install : all
	@mkdir -p "$(DESTDIR)$(PREFIX)/mod"
	@echo "INSTALL $(MOD_KO)"
	$(Q)cp "$(BUILDDIR)/$(MOD_KO)" "$(DESTDIR)$(PREFIX)/mod/"

uninstall :
	@echo "UNINSTALL $(MOD_KO)"
	$(Q)rm -f "$(DESTDIR)$(PREFIX)/mod/$(MOD_KO)"

clean :
	@echo "CLEAN $(BUILDDIR)"
	$(Q) rm -rf "$(BUILDDIR)"

.PHONY : all install uninstall clean
