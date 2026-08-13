# makefile include to build a library

LIB ?= $(PACKAGE)
SRCS ?= $(wildcard *.[cs])
INCS ?= $(wildcard include/*)
STATIC ?= yes
SHARED ?= yes
STATIC_LIB ?= lib$(LIB).a
SHARED_LIB ?= lib$(LIB).so
STATIC_OBJS += $(SRCS:%=$(BUILDDIR)/%.o)
SHARED_OBJS += $(SRCS:%=$(BUILDDIR)/%.so)
CFLAGS := -std=c99 -I ./ $(CFLAGS)
LIBMODE ?= 0744

all :

include $(TMAKE_DIR)/tmake-compile.mk

ifneq ($(STATIC),no)
all : $(BUILDDIR)/$(STATIC_LIB)
$(BUILDDIR)/$(STATIC_LIB) : $(STATIC_OBJS)
	@mkdir -p "$(@D)"
	@echo "AR $(STATIC_LIB)"
	$(Q)$(AR) rcs "$@" $^

install : install-static
install-static : $(BUILDDIR)/$(STATIC_LIB)
	@mkdir -p "$(DESTDIR)$(LIBDIR)"
	@echo "INSTALL $(STATIC_LIB)"
	$(Q)$(INSTALL) -m $(LIBMODE) "$(BUILDDIR)/$(STATIC_LIB)" "$(DESTDIR)$(LIBDIR)/"

uninstall : uninstall-static
uninstall-static :
	@echo "UNINSTALL $(STATIC_LIB)"
	$(Q)rm -f "$(DESTDIR)$(LIBDIR)/$(STATIC_LIB)"
endif

ifneq ($(SHARED),no)
all : $(BUILDDIR)/$(SHARED_LIB)
$(BUILDDIR)/$(SHARED_LIB) : $(SHARED_OBJS)
	@mkdir -p "$(@D)"
	@echo "CCLD $(SHARED_LIB)"
	$(Q)$(CC) -shared $(CFLAGS) -Wl,-soname,$(SHARED_LIB) -o "$@" $^ $(LDFLAGS)

install : install-shared
install-shared : $(BUILDDIR)/$(SHARED_LIB)
	@mkdir -p "$(DESTDIR)$(LIBDIR)"
	@echo "INSTALL $(SHARED_LIB)"
	$(Q)$(INSTALL) -m $(LIBMODE) "$(BUILDDIR)/$(SHARED_LIB)" "$(DESTDIR)$(LIBDIR)/"

uninstall : uninstall-shared
uninstall-shared :
	@echo "UNINSTALL $(SHARED_LIB)"
	$(Q)rm -f "$(DESTDIR)$(LIBDIR)/$(SHARED_LIB)"
endif

clean :
	@echo "CLEAN $(BUILDDIR)"
	$(Q) rm -rf "$(BUILDDIR)"

include $(TMAKE_DIR)/tmake-incs.mk

.PHONY : install install-static install-shared uninstall uninstall-static uninstall-shared clean
