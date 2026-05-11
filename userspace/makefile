MAKEFLAGS += --no-builtin-rules
PREFIX = /
BUILDDIR ?= $(realpath build)

ifeq ($(TOP),)
CFLAGS += -O2 -g
LDFLAGS += -g
export CFLAGS
endif

#first get all the sub directory
APPS = $(shell find -name "*" -not -name "bin" -not -path "." -type d -maxdepth 1 -exec basename \{} \;)

all :
	@mkdir -p $(BUILDDIR)
	@$(foreach DIR,$(APPS),mkdir -p $(BUILDDIR)/$(DIR) && $(MAKE) -C $(DIR) BUILDDIR=$(BUILDDIR)/$(DIR) &&) true

# install everything
install : all
	@mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	$(foreach APP,$(APPS),cp "$(BUILDDIR)/$(APP)/$(APP)" "$(DESTDIR)$(PREFIX)/bin" ;) true
	@$(MAKE) -C tutils BUILDDIR="$(BUILDDIR)/tutils" PREFIX=/usr install

clean :
	@$(foreach DIR,$(APPS),$(MAKE) -C $(DIR) clean && ) true
