MAKEFLAGS += --no-builtin-rules
PREFIX = ../initrd

ifeq ($(TOP),)
CFLAGS += -O2 -g
LDFLAGS += -g
export CFLAGS
endif

#first get all the sub directory
APPS = $(shell find -name "*" -not -name "bin" -not -path "." -type d -maxdepth 1 -exec basename \{} \;)

all :
	@mkdir -p bin
	@$(foreach DIR,$(APPS),$(MAKE) -C $(DIR) SYSROOT=$(SYSROOT) &&) true

#for installing to the initrd
install : all
	@mkdir -p $(PREFIX)/bin
	cp $(foreach APP,$(APPS),bin/$(APP)) $(PREFIX)/bin
clean :
	@$(foreach DIR,$(APPS),$(MAKE) -C $(DIR) clean SYSROOT=$(SYSROOT) && ) true