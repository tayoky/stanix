TOP ?= $(CURDIR)/..
TMAKE_DIR ?= $(TOP)/make
include $(TMAKE_DIR)/tmake-init.mk

SUBDIRS = $(patsubst %/,%,$(dir $(wildcard */Makefile)))

include $(TMAKE_DIR)/tmake-subdir.mk

# install headers
INCS = $(wildcard include/module/*.h)
INCSDIR := $(INCSDIR)/module
include $(TMAKE_DIR)/tmake-incs.mk
