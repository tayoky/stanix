# makefile include to build a library

SRCS ?= $(wildcard *.[cs])
HEADERS ?= $(wildcard include/*)
STATIC_LIB ?= lib$(LIB).a
SHARED_LIB ?= lib$(LIB).so
STATIC_OBJS += $(SRCS:%=$(BUILDDIR)/%.o)
SHARED_OBJS += $(SRCS:%=$(BUILDDIR)/%.so)
CFLAGS := -std=c99 -I ./ $(CFLAGS)

all :

include $(TMAKE_DIR)/tmake-compile.mk

ifneq ($(strip $(STATIC_OBJS)),)
all : $(BUILDDIR)/$(STATIC_LIB)
$(BUILDDIR)/$(STATIC_LIB) : $(STATIC_OBJS)
	@mkdir -p "$(@D)"
	@echo "AR $(STATIC_LIB)"
	$(Q)$(AR) rcs "$@" $^

install : install-static
install-static : $(BUILDDIR)/$(STATIC_LIB)
	@mkdir -p "$(DESTDIR)$(PREFIX)/lib"
	@echo "INSTALL $(STATIC_LIB)"
	$(Q)cp "$(BUILDDIR)/$(STATIC_LIB)" "$(DESTDIR)$(PREFIX)/lib/"
endif

ifneq ($(strip $(SHARED_OBJS)),)
all : $(BUILDDIR)/$(SHARED_LIB)
$(BUILDDIR)/$(SHARED_LIB) : $(SHARED_OBJS)
	@mkdir -p "$(@D)"
	@echo "CCLD $(SHARED_LIB)"
	$(Q)$(CC) -shared $(CFLAGS) -o "$@" $^ $(LDFLAGS)

install : install-shared
install-shared : $(BUILDDIR)/$(SHARED_LIB)
	@mkdir -p "$(DESTDIR)$(PREFIX)/lib"
	@echo "INSTALL $(SHARED_LIB)"
	$(Q)cp "$(BUILDDIR)/$(SHARED_LIB)" "$(DESTDIR)$(PREFIX)/lib/"
endif

ifneq ($(strip $(HEADERS)),)
install : install-headers
install-headers :
	@mkdir -p "$(DESTDIR)$(PREFIX)/include"
	@echo "INSTALL $(HEADERS)"
	$(Q)cp -r $(HEADERS) "$(DESTDIR)$(PREFIX)/include/"

endif

clean :
	@echo "CLEAN $(BUILDDIR)"
	$(Q) rm -rf "$(BUILDDIR)"

.PHONY : all install-headers install-static install-shared install clean
