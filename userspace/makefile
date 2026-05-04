MAKEFLAGS += --no-builtin-rules
PREFIX = /

ifeq ($(TOP),)
CFLAGS += -O2 -g
LDFLAGS += -g
export CFLAGS
endif

#first get all the sub directory
APPS = $(shell find -name "*" -not -name "bin" -not -path "." -type d -maxdepth 1 -exec basename \{} \;)

all :
	@mkdir -p bin
	@$(foreach DIR,$(APPS),$(MAKE) -C $(DIR) &&) true

# install everything
install : all
	@mkdir -p "$(DESTDIR)$(PREFIX)/bin"
	cp $(foreach APP,$(APPS),bin/$(APP)) "$(DESTDIR)$(PREFIX)/bin"
clean :
	@$(foreach DIR,$(APPS),$(MAKE) -C $(DIR) clean && ) true