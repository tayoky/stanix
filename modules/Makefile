TOP ?= $(CURDIR)/..
TMAKE_DIR ?= $(TOP)/make
include $(TMAKE_DIR)/tmake-init.mk

SUBDIRS = $(patsubst %/,%,$(dir $(wildcard */Makefile)))

include $(TMAKE_DIR)/tmake-subdir.mk

# install headers
FILES = include/module
FILESDIR = $(INCLUDEDIR)
include $(TMAKE_DIR)/tmake-files.mk

.PHONY : install-headers
install-headers : install-files
.PHONY : uninstall-headers
uninstall-headers : uninstall-files
