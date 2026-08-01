TOP ?= $(CURDIR)/..
TMAKE_DIR ?= $(TOP)/make
include $(TMAKE_DIR)/tmake-init.mk

SUBDIRS = $(patsubst %/,%,$(dir $(wildcard */Makefile)))

include $(TMAKE_DIR)/tmake-subdir.mk

# dependencies
all-libtgui install-libtgui : install-libgfx install-libtwm
all-libini  install-libini  : install-libutils
all-libtwm  install-libtwm  : install-libgfx
